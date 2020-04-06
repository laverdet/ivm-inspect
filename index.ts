import ivm from 'isolated-vm';
import binding from './binding';
type Binding = typeof binding;

const nativeModule = new ivm.NativeModule(require.resolve(binding.path));

const kInspect = 'internal/util/inspect';
const kPrimordials = 'internal/per_context/primordials';
const kFiles = [
	kPrimordials,
	kInspect,
	'internal/errors',
	'internal/util',
	'internal/util/types',
];

export async function create(isolate: ivm.Isolate, context: ivm.Context) {
	const [ modules, binding, primordialsHolder ] = await Promise.all([
		// Compile modules
		async function() {
			return new Map(await Promise.all(kFiles.map(async module => {
				const src: string = require(`../nodejs/lib/${module}`);
				const ivmSrc = `(function(global,primordials,internalBinding,process,module,require){${src}})`;
				const script = await isolate.compileScript(ivmSrc, { filename: `${module}.js` });
				const reference = await script.run(context, { release: true, reference: true });
				const entry: [ string, ivm.Reference ] = [ module, reference ];
				return entry;
			})));
		}(),
		// Binding
		async function() {
			const moduleInstance: ivm.Reference<Binding> = await nativeModule.create(context);
			const makeBinding = await moduleInstance.get('makeBinding');
			const binding = await makeBinding.apply();
			moduleInstance.release();
			makeBinding.release();
			return binding;
		}(),
		// Make primordials holder
		context.eval('Object.create(null)', { reference: true }),
	]);

	// Run primordials.js
	const { global } = context;
	const primordials = primordialsHolder.result;
	await modules.get(kPrimordials)!.apply(undefined, [ global.derefInto(), primordials.derefInto()	]);

	// Continue with bootstrap
	const { result } = await context.evalClosure(`
		const process = {
			versions: {},
		};
		const internalBinding = name => $3[name];
		const cache = new Map;
		function require(name) {
			const cached = cache.get(name);
			if (cached !== undefined) {
				return cached.exports;
			}
			const module = { exports: {} };
			cache.set(name, module);
			const fn = $0.get(name);
			if (fn) {
				fn.deref()($1, $2, internalBinding, process, module, require);
			}
			return module.exports;
		}
		return require("${kInspect}");
	`, [
		new ivm.ExternalCopy(modules).copyInto(),
		global.derefInto(),
		primordials.derefInto({ release: true }),
		binding.derefInto({ release: true }),
	], { result: { reference: true } });
	const util: ivm.Reference<typeof import('util')> = result;

	// Export functions
	const [ formatWithOptions, inspect ] = await Promise.all([
		util.get('formatWithOptions'),
		util.get('inspect'),
	]);

	// Release remaining handles
	modules.forEach(module => module.release());
	util.release();
	return { formatWithOptions, inspect };
}

export async function forwardConsole(
	context: ivm.Context,
	util: ReturnType<typeof create> extends Promise<infer T> ? T : never
) {
	const print = (fd: number, payload: string) => {
		const stream = fd === 2 ? process.stderr : process.stdout;
		stream.write(payload + '\n');
	};
	context.evalClosureSync(`
		const [ formatWithOptions, inspect ] = [ $0, $1 ];
		const kTraceBegin = 'b'.charCodeAt(0);
		const kTraceEnd = 'e'.charCodeAt(0);
		const kTraceInstant = 'n'.charCodeAt(0);
		const times = new Map;
		const write = (fd, payload) => $2.applySync(undefined, [ fd, payload ]);
		const format = args => formatWithOptions({ colors: true }, ...args);
		Object.assign(console, {
			log(...args) {
				write(1, format(args));
			},

			warn(...args) {
				write(2, format(args));
			},

			error(...args) {
				this.warn(...args);
			},

			dir(object, options) {
				write(1, inspect(object, {
					customInspect: false,
					color: true,
					...options,
				}));
			},

			trace: function trace(...args) {
				const err = {
					name: 'Trace',
					message: format(args),
				};
				Error.captureStackTrace(err, trace);
				this.error(err.stack);
			},

			assert(expression, ...args) {
				if (!expression) {
					args[0] = \`Assertion failed\${args.length === 0 ? '' : \`: \${args[0]}\`}\`;
					this.warn(...args); // The arguments will be formatted in warn() again
				}
			},
		});
		`,
		[
			util.formatWithOptions.derefInto(),
			util.inspect.derefInto(),
			new ivm.Reference(print),
		],
	);
}
