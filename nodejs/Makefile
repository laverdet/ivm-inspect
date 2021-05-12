# NOTE: Be sure to run this with NODE_SRC environment variable set

# nodejs files we need
SRCS := \
	lib/internal/bootstrap/loaders.js \
	lib/internal/errors.js \
	lib/internal/per_context/primordials.js \
	lib/internal/util.js \
	lib/internal/util/inspect.js \
	lib/internal/util/types.js
TARGETS := $(patsubst %.js,%.json,$(SRCS))

# Dump JS to JSON array
lib/%.json: $(NODE_SRC)/lib/%.js
	mkdir -p $$(dirname $@)
	node -e 'console.log(JSON.stringify(fs.readFileSync(process.argv[1], "utf8")))' $^ > $@

all: $(TARGETS)
