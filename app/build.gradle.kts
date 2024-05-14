plugins {
	alias(libs.plugins.android.application)
	alias(libs.plugins.jetbrains.kotlin.android)
}

android {
	namespace = "vn.name.leduyquang753.yubinobutai"
	compileSdk = 34
	ndkVersion = "26.3.11579264"

	defaultConfig {
		applicationId = "vn.name.leduyquang753.yubinobutai"
		minSdk = 30
		targetSdk = 34
		versionCode = 1
		versionName = "0.0.1"

		testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
		externalNativeBuild {
			cmake {
				arguments += "-DANDROID_STL=c++_shared"
				cppFlags += "-std=c++17"
			}
		}
	}

	buildTypes {
		debug {
			ndk {
				abiFilters += "arm64-v8a"
			}
		}
		release {
			isMinifyEnabled = false
			proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
		}
	}
	compileOptions {
		sourceCompatibility = JavaVersion.VERSION_1_8
		targetCompatibility = JavaVersion.VERSION_1_8
	}
	kotlinOptions {
		jvmTarget = "1.8"
	}
	buildFeatures {
		prefab = true
	}
	externalNativeBuild {
		cmake {
			path = file("src/main/cpp/CMakeLists.txt")
			version = "3.22.1"
		}
	}
	sourceSets {
		getByName("main") {
			jniLibs.srcDirs(arrayOf("../FFmpeg/libs"))
		}
	}
}

dependencies {
	implementation(libs.androidx.appcompat)
	implementation(libs.androidx.core)
	implementation(libs.androidx.core.ktx)
	implementation(libs.androidx.games.activity)
	implementation(libs.material)
	implementation(libs.oboe)
	testImplementation(libs.junit)
	androidTestImplementation(libs.androidx.junit)
	androidTestImplementation(libs.androidx.espresso.core)
}