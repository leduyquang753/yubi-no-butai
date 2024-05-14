package vn.name.leduyquang753.yubinobutai;

import com.google.androidgamesdk.GameActivity;

class MainActivity: GameActivity() {
	companion object {
		init {
			System.loadLibrary("yubinobutai");
		}
	}

	override fun onWindowFocusChanged(hasFocus: Boolean) {
		super.onWindowFocusChanged(hasFocus);
	}
}