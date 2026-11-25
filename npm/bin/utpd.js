#!/usr/bin/env node
'use strict';

const { launchBinary } = require('./launcher');

launchBinary({
  names: ['utpd', 'utp-server'],
  label: 'UTPD',
  installHint: "ensure 'utpd' is compiled or available in your PATH.",
  scriptPath: __filename
});
