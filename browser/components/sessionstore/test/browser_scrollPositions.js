/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const BASE = "http://example.com/browser/browser/components/sessionstore/test/";
const URL = BASE + "browser_scrollPositions_sample.html";
const URL2 = BASE + "browser_scrollPositions_sample2.html";
const URL_FRAMESET = BASE + "browser_scrollPositions_sample_frameset.html";

// Randomized set of scroll positions we will use in this test.
const SCROLL_X = Math.round(100 * (1 + Math.random()));
const SCROLL_Y = Math.round(200 * (1 + Math.random()));
const SCROLL_STR = SCROLL_X + "," + SCROLL_Y;

const SCROLL2_X = Math.round(300 * (1 + Math.random()));
const SCROLL2_Y = Math.round(400 * (1 + Math.random()));
const SCROLL2_STR = SCROLL2_X + "," + SCROLL2_Y;

requestLongerTimeout(2);

/**
 * This test ensures that we properly serialize and restore scroll positions
 * for an average page without any frames.
 */
add_task(async function test_scroll() {
  let tab = BrowserTestUtils.addTab(gBrowser, URL);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Scroll down a little.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y});
  await checkScroll(tab, {scroll: SCROLL_STR}, "scroll is fine");

  // Duplicate and check that the scroll position is restored.
  let tab2 = ss.duplicateTab(window, tab);
  let browser2 = tab2.linkedBrowser;
  await promiseTabRestored(tab2);

  let scroll = await sendMessage(browser2, "ss-test:getScrollPosition");
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL_X, y: SCROLL_Y}),
    "scroll position has been duplicated correctly");

  // Check that reloading retains the scroll positions.
  browser2.reload();
  await promiseBrowserLoaded(browser2);
  await checkScroll(tab2, {scroll: SCROLL_STR}, "reloading retains scroll positions");

  // Check that a force-reload resets scroll positions.
  browser2.reloadWithFlags(Ci.nsIWebNavigation.LOAD_FLAGS_BYPASS_CACHE);
  await promiseBrowserLoaded(browser2);
  await checkScroll(tab2, null, "force-reload resets scroll positions");

  // Scroll back to the top and check that the position has been reset. We
  // expect the scroll position to be "null" here because there is no data to
  // be stored if the frame is in its default scroll position.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0});
  await checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

/**
 * This tests ensures that we properly serialize and restore scroll positions
 * for multiple frames of pages with framesets.
 */
add_task(async function test_scroll_nested() {
  let tab = BrowserTestUtils.addTab(gBrowser, URL_FRAMESET);
  let browser = tab.linkedBrowser;
  await promiseBrowserLoaded(browser);

  // Scroll the first child frame down a little.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y, frame: 0});
  await checkScroll(tab, {children: [{scroll: SCROLL_STR}]}, "scroll is fine");

  // Scroll the second child frame down a little.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL2_X, y: SCROLL2_Y, frame: 1});
  await checkScroll(tab, {children: [{scroll: SCROLL_STR}, {scroll: SCROLL2_STR}]}, "scroll is fine");

  // Duplicate and check that the scroll position is restored.
  let tab2 = ss.duplicateTab(window, tab);
  let browser2 = tab2.linkedBrowser;
  await promiseTabRestored(tab2);

  let scroll = await sendMessage(browser2, "ss-test:getScrollPosition", {frame: 0});
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL_X, y: SCROLL_Y}),
    "scroll position #1 has been duplicated correctly");

  scroll = await sendMessage(browser2, "ss-test:getScrollPosition", {frame: 1});
  is(JSON.stringify(scroll), JSON.stringify({x: SCROLL2_X, y: SCROLL2_Y}),
    "scroll position #2 has been duplicated correctly");

  // Check that resetting one frame's scroll position removes it from the
  // serialized value.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0, frame: 0});
  await checkScroll(tab, {children: [null, {scroll: SCROLL2_STR}]}, "scroll is fine");

  // Check the resetting all frames' scroll positions nulls the stored value.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: 0, y: 0, frame: 1});
  await checkScroll(tab, null, "no scroll stored");

  // Cleanup.
  BrowserTestUtils.removeTab(tab);
  BrowserTestUtils.removeTab(tab2);
});

/**
 * Test that scroll positions persist after restoring background tabs in
 * a restored window (bug 1228518).
 * Also test that scroll positions for previous session history entries
 * are preserved as well (bug 1265818).
 */
add_task(async function test_scroll_background_tabs() {
  pushPrefs(["browser.sessionstore.restore_on_demand", true]);

  let newWin = await BrowserTestUtils.openNewBrowserWindow();
  let tab = BrowserTestUtils.addTab(newWin.gBrowser, URL);
  let browser = tab.linkedBrowser;
  await BrowserTestUtils.browserLoaded(browser);

  // Scroll down a little.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL_X, y: SCROLL_Y});
  await checkScroll(tab, {scroll: SCROLL_STR}, "scroll on first page is fine");

  // Navigate to a different page and scroll there as well.
  browser.loadURI(URL2);
  await BrowserTestUtils.browserLoaded(browser);

  // Scroll down a little.
  await sendMessage(browser, "ss-test:setScrollPosition", {x: SCROLL2_X, y: SCROLL2_Y});
  await checkScroll(tab, {scroll: SCROLL2_STR}, "scroll on second page is fine");

  // Close the window
  await BrowserTestUtils.closeWindow(newWin);

  await forceSaveState();

  // Now restore the window
  newWin = ss.undoCloseWindow(0);

  // Make sure to wait for the window to be restored.
  await BrowserTestUtils.waitForEvent(newWin, "SSWindowStateReady");

  is(newWin.gBrowser.tabs.length, 2, "There should be two tabs");

  // The second tab should be the one we loaded URL at still
  tab = newWin.gBrowser.tabs[1];

  ok(tab.hasAttribute("pending"), "Tab should be pending");
  browser = tab.linkedBrowser;

  // Ensure there are no pending queued messages in the child.
  await TabStateFlusher.flush(browser);

  // Now check to see if the background tab remembers where it
  // should be scrolled to.
  newWin.gBrowser.selectedTab = tab;
  await promiseTabRestored(tab);

  await checkScroll(tab, {scroll: SCROLL2_STR}, "scroll is still fine");

  // Now go back in history and check that the scroll position
  // is restored there as well.
  is(browser.canGoBack, true, "can go back");
  browser.goBack();

  await BrowserTestUtils.browserLoaded(browser);
  await TabStateFlusher.flush(browser);

  await checkScroll(tab, {scroll: SCROLL_STR}, "scroll is still fine after navigating back");

  await BrowserTestUtils.closeWindow(newWin);
});
