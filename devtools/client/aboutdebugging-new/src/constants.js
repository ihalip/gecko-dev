/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const actionTypes = {
  CONNECT_RUNTIME_FAILURE: "CONNECT_RUNTIME_FAILURE",
  CONNECT_RUNTIME_START: "CONNECT_RUNTIME_START",
  CONNECT_RUNTIME_SUCCESS: "CONNECT_RUNTIME_SUCCESS",
  DISCONNECT_RUNTIME_FAILURE: "DISCONNECT_RUNTIME_FAILURE",
  DISCONNECT_RUNTIME_START: "DISCONNECT_RUNTIME_START",
  DISCONNECT_RUNTIME_SUCCESS: "DISCONNECT_RUNTIME_SUCCESS",
  REQUEST_TABS_FAILURE: "REQUEST_TABS_FAILURE",
  REQUEST_TABS_START: "REQUEST_TABS_START",
  REQUEST_TABS_SUCCESS: "REQUEST_TABS_SUCCESS",
  PAGE_SELECTED: "PAGE_SELECTED",
};

const DEBUG_TARGETS = {
  TAB: "TAB",
};

const PAGES = {
  THIS_FIREFOX: "this-firefox",
  CONNECT: "connect",
};

// flatten constants
module.exports = Object.assign({}, { PAGES }, actionTypes, { DEBUG_TARGETS });
