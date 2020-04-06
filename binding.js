'use strict';
const path = '../build/Release/binding.node';
module.exports.path = path;
module.exports.makeBinding = require(path).makeBinding;
