/*
 * Copyright (C) 2020 YAAP
 * Copyright (C) 2023-2024 cyberknight777
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.fastcharge.battery;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.os.Handler;
import android.os.UserHandle;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceManager;
import androidx.preference.SwitchPreferenceCompat;

import com.android.fastcharge.R;
import com.android.fastcharge.utils.FileUtils;

public class FastChargeFragment extends PreferenceFragmentCompat implements
        Preference.OnPreferenceChangeListener {

    private SwitchPreferenceCompat mFastChargePreference;
    private FastChargeConfig mConfig;
    private boolean mInternalFastChargeStart = false;

    private final BroadcastReceiver mServiceStateReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (action.equals(mConfig.ACTION_FAST_CHARGE_SERVICE_CHANGED)) {
                if (mInternalFastChargeStart) {
                        mInternalFastChargeStart = false;
                        return;
                }

                if (mFastChargePreference == null) return;

                final boolean fastchargeStarted = intent.getBooleanExtra(
                            mConfig.EXTRA_FAST_CHARGE_STATE, false);

                mFastChargePreference.setChecked(fastchargeStarted);

            }
        }
    };

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        setPreferencesFromResource(R.xml.fastcharge_settings, rootKey);
        mConfig = FastChargeConfig.getInstance(getContext());
        mFastChargePreference = (SwitchPreferenceCompat) findPreference(mConfig.FASTCHARGE_KEY);
        if (FileUtils.fileExists(mConfig.getFastChargePath())) {
            mFastChargePreference.setEnabled(true);
            mFastChargePreference.setOnPreferenceChangeListener(this);
        } else {
            mFastChargePreference.setSummary(R.string.fast_charging_summary_not_supported);
            mFastChargePreference.setEnabled(false);
        }

        mFastChargePreference.setChecked(mConfig.isCurrentlyEnabled(mConfig.getFastChargePath()));

        // Registering observers
        IntentFilter filter = new IntentFilter();
        filter.addAction(mConfig.ACTION_FAST_CHARGE_SERVICE_CHANGED);
        getContext().registerReceiver(mServiceStateReceiver, filter, Context.RECEIVER_EXPORTED);
    }

    @Override
    public void onResume() {
        super.onResume();
        mFastChargePreference.setChecked(mConfig.isCurrentlyEnabled(mConfig.getFastChargePath()));
    }


    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (mConfig.FASTCHARGE_KEY.equals(preference.getKey())) {
            mInternalFastChargeStart = true;
            Context mContext = getContext();

            SharedPreferences sharedPrefs = PreferenceManager.getDefaultSharedPreferences(mContext);

            FileUtils.writeLine(mConfig.getFastChargePath(), (Boolean) newValue ? "1":"0");

            boolean enabled = mConfig.isCurrentlyEnabled(mConfig.getFastChargePath());

            sharedPrefs.edit().putBoolean(mConfig.FASTCHARGE_KEY, enabled).commit();

            Intent intent = new Intent(mConfig.ACTION_FAST_CHARGE_SERVICE_CHANGED);

            intent.putExtra(mConfig.EXTRA_FAST_CHARGE_STATE, enabled);
            intent.setFlags(Intent.FLAG_RECEIVER_REGISTERED_ONLY);
            mContext.sendBroadcastAsUser(intent, UserHandle.CURRENT);;
        }
        return true;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        getContext().unregisterReceiver(mServiceStateReceiver);
    }

}
