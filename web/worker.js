// Copyright 2024 TIsland Crew
// SPDX-License-Identifier: Apache-2.0

// t2esx WebWorker thread

'use strict';

// for split.js
// add ?r=' + Math.random()); in dev environment to ensure reload
// and ?r=RELTAG'); for released version
importScripts('wav.js', 'tzx.js', 'split.js');

onmessage = t2esx.onmessage;

// EOF vim: et:ai:ts=4:sw=4:
