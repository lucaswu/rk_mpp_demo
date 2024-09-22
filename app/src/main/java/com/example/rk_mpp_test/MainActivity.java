package com.example.rk_mpp_test;

import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.util.Log;
import android.widget.TextView;

import com.example.rk_mpp_test.databinding.ActivityMainBinding;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'rk_mpp_test' library on application startup.
    static {
        System.loadLibrary("rk_mpp_test");
    }

    private ActivityMainBinding binding;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        binding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(binding.getRoot());



        String dirUrl = getApplicationContext().getExternalCacheDir().getAbsolutePath();
        Log.i("lucas","dirUrl:"+dirUrl);

//          testDecoder();
        
        testEncoder();

    }



    public native void testDecoder();
    public native void testEncoder();
}