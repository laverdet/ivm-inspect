[![npm version](https://badgen.now.sh/npm/v/ivm-inspect)](https://www.npmjs.com/package/ivm-inspect)
[![mit license](https://badgen.now.sh/npm/license/ivm-inspect)](https://github.com/laverdet/ivm-inspect/blob/master/LICENSE)
[![npm downloads](https://badgen.now.sh/npm/dm/ivm-inspect)](https://www.npmjs.com/package/ivm-inspect)

ivm-inspect -- Local console support within isolated-vm
====================================================

[![NPM](https://nodei.co/npm/ivm-inspect.png)](https://www.npmjs.com/package/ivm-inspect)

This is library meant to be used in conjunction with
[`isolated-vm`](https://github.com/laverdet/isolated-vm). It adds rudimentary console support for
the common case of outputting to a local console. I would guess it's probably pretty easy to crash
the process by passing whacky objects to these functions so use this at your own risk.


API DOCUMENTATION
-----------------

#### `async create(isolate, context)`
* `isolate` - An [isolate](https://github.com/laverdet/isolated-vm#class-isolate-transferable) created within `isolated-vm`
* `context` - A [context](https://github.com/laverdet/isolated-vm#class-context-transferable)
* *return* `{ formatWithOptions, inspect }`

This returns an object with two named `Reference` instances to the built-in nodejs functions
[`util.formatWithOptions`](https://nodejs.org/api/util.html#util_util_formatwithoptions_inspectoptions_format_args)
and [`util.inspect`](https://nodejs.org/api/util.html#util_util_inspect_object_options). These
references belong to the isolate and maintain internal handles to the context. You can use these
functions in any context created within this isolate-- you do not need to call this once per
context.


#### `async forwardConsole(context, util)`
* `context` - A [context](https://github.com/laverdet/isolated-vm#class-context-transferable)
* `util` - The return value of `await create(...)`

This function configures a context's `console` to output directly to the process's stdout and
stderr.


EXAMPLES
--------

```js
const ivm = require('isolated-vm');
const ivmInspect = require('ivm-inspect');

(async() => {
	const isolate = new ivm.Isolate;
	const context = await isolate.createContext();

	const util = await ivmInspect.create(isolate, context);
	await ivmInspect.forwardConsole(context, util);

	await context.eval('console.log("Here is an object: %O", { foo: "bar" })');
})().catch(console.error);
```

![console output](https://i.imgur.com/bO55xsh.png)

Wow!
