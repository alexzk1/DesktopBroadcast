package biz.an_droid.br_client;

import android.os.Bundle;

import com.badlogic.gdx.backends.android.AndroidApplication;
import com.badlogic.gdx.backends.android.AndroidApplicationConfiguration;
import biz.an_droid.br_client.MainBroadClient;

public class AndroidLauncher extends AndroidApplication
{
    @Override
    protected void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);
        AndroidApplicationConfiguration config = new AndroidApplicationConfiguration();
        initialize(new MainBroadClient(), config);
    }
}
