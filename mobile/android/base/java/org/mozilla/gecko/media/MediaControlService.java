/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
package org.mozilla.gecko.media;

import android.annotation.SuppressLint;
import android.app.Notification;
import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

import org.mozilla.gecko.AppConstants;
import org.mozilla.gecko.R;
import org.mozilla.gecko.notifications.NotificationHelper;

public class MediaControlService extends Service {
    private static final String LOGTAG = "MediaControlService";

    private Notification currentNotification;

    @SuppressLint("NewApi")
    @Override
    public void onCreate() {
        super.onCreate();
        // Initialize our current notification as a blank notification for cases when the service is started directly with the shutdown
        // action before being started with a valid notification.
        if (AppConstants.Versions.preO) {
            currentNotification = new Notification.Builder(this).build();
        } else {
            currentNotification = new Notification.Builder(this, NotificationHelper.getInstance(this)
                    .getNotificationChannel(NotificationHelper.Channel.DEFAULT).getId()).build();
        }
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Log.d(LOGTAG, "onStartCommand");

        if (intent.hasExtra(GeckoMediaControlAgent.EXTRA_NOTIFICATION_DATA)) {
            currentNotification = GeckoMediaControlAgent.getInstance().createNotification(
                    (MediaNotification) intent.getParcelableExtra(GeckoMediaControlAgent.EXTRA_NOTIFICATION_DATA));
        }

        startForeground(R.id.mediaControlNotification, currentNotification);

        if (intent.getAction() != null) {
            final String action = intent.getAction();
            if (action.equals(GeckoMediaControlAgent.ACTION_SHUTDOWN)) {
                stopForeground(true);
                stopSelfResult(startId);
            } else {
                GeckoMediaControlAgent.getInstance().handleAction(action);
            }
        }

        return START_NOT_STICKY;
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
