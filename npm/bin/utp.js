#!/usr/bin/env node
'use strict';

const { launchBinary } = require('./launcher');

launchBinary({
  names: ['utp'],
  label: 'UTP',
  installHint: "ensure 'utp' is compiled or available in your PATH.",
  scriptPath: __filename
});
