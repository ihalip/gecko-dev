/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* Bug 646070 */

var DEVTOOLS_CHROME_ENABLED = "devtools.chrome.enabled";

function test() {
  waitForExplicitFinish();

  Services.prefs.setBoolPref(DEVTOOLS_CHROME_ENABLED, true);

  gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser);
  BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser).then(function() {
    openScratchpad(runTests);
  });

  gBrowser.loadURI("data:text/html,Scratchpad test for bug 646070 - chrome context preference");
}

function runTests() {
  const sp = gScratchpadWindow.Scratchpad;
  ok(sp, "Scratchpad object exists in new window");

  const environmentMenu = gScratchpadWindow.document
                          .getElementById("sp-environment-menu");
  ok(environmentMenu, "Environment menu element exists");
  ok(!environmentMenu.hasAttribute("hidden"),
     "Environment menu is visible");

  const errorConsoleCommand = gScratchpadWindow.document
                            .getElementById("sp-cmd-errorConsole");
  ok(errorConsoleCommand, "Error console command element exists");
  ok(!errorConsoleCommand.hasAttribute("disabled"),
     "Error console command is enabled");

  const chromeContextCommand = gScratchpadWindow.document
                            .getElementById("sp-cmd-browserContext");
  ok(chromeContextCommand, "Chrome context command element exists");
  ok(!chromeContextCommand.hasAttribute("disabled"),
     "Chrome context command is disabled");

  Services.prefs.clearUserPref(DEVTOOLS_CHROME_ENABLED);

  finish();
}
