/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let HOMEPAGE_PREF = "browser.startup.homepage";

  await pushPrefs([HOMEPAGE_PREF, "about:mozilla"]);

  let EventUtils = {};
  Services.scriptloader.loadSubScript("chrome://mochikit/content/tests/SimpleTest/EventUtils.js", EventUtils);

  // Since synthesizeDrop triggers the srcElement, need to use another button.
  let dragSrcElement = document.getElementById("downloads-button");
  ok(dragSrcElement, "Downloads button exists");
  let homeButton = document.getElementById("home-button");
  ok(homeButton, "home button present");

  async function drop(dragData, homepage) {
    let setHomepageDialogPromise = BrowserTestUtils.domWindowOpened();

    EventUtils.synthesizeDrop(dragSrcElement, homeButton, dragData, "copy", window);

    let setHomepageDialog = await setHomepageDialogPromise;
    ok(true, "dialog appeared in response to home button drop");
    await BrowserTestUtils.waitForEvent(setHomepageDialog, "load", false);

    let setHomepagePromise = new Promise(function(resolve) {
      let observer = {
        QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),
        observe(subject, topic, data) {
          is(topic, "nsPref:changed", "observed correct topic");
          is(data, HOMEPAGE_PREF, "observed correct data");
          let modified = Services.prefs.getStringPref(HOMEPAGE_PREF);
          is(modified, homepage, "homepage is set correctly");
          Services.prefs.removeObserver(HOMEPAGE_PREF, observer);

          Services.prefs.setStringPref(HOMEPAGE_PREF, "about:mozilla;");

          resolve();
        }
      };
      Services.prefs.addObserver(HOMEPAGE_PREF, observer);
    });

    setHomepageDialog.document.documentElement.acceptDialog();

    await setHomepagePromise;
  }

  function dropInvalidURI() {
    return new Promise(resolve => {
      let consoleListener = {
        observe(m) {
          if (m.message.includes("NS_ERROR_DOM_BAD_URI")) {
            ok(true, "drop was blocked");
            resolve();
          }
        }
      };
      Services.console.registerListener(consoleListener);
      registerCleanupFunction(function() {
        Services.console.unregisterListener(consoleListener);
      });

      executeSoon(function() {
        info("Attempting second drop, of a javascript: URI");
        // The drop handler throws an exception when dragging URIs that inherit
        // principal, e.g. javascript:
        expectUncaughtException();
        EventUtils.synthesizeDrop(dragSrcElement, homeButton, [[{type: "text/plain", data: "javascript:8888"}]], "copy", window);
      });
    });
  }

  await drop([[{type: "text/plain",
                 data: "http://mochi.test:8888/"}]],
              "http://mochi.test:8888/");
  await drop([[{type: "text/plain",
                 data: "http://mochi.test:8888/\nhttp://mochi.test:8888/b\nhttp://mochi.test:8888/c"}]],
              "http://mochi.test:8888/|http://mochi.test:8888/b|http://mochi.test:8888/c");
  await dropInvalidURI();
});
