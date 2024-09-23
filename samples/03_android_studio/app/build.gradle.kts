plugins {
    alias(libs.plugins.android.application)
}

android {
    namespace = "com.imapp.sample_android_studio"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.imapp.sample_android_studio"
        minSdk = 24
        targetSdk = 34
        versionCode = 1
        versionName = "1.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
        }
    }
    externalNativeBuild {
        cmake {
            path = file("src/main/cpp/CMakeLists.txt")
        }
    }
    buildFeatures {
        viewBinding = true
    }
}
