plugins {
    id("com.android.application")
    id("org.jetbrains.kotlin.android")
}

// The app shows the device's own web interface (web/index.html, the source of include/PortalPage.h)
// and carries the firmware version it was built with.
val repo = rootProject.projectDir.parentFile
val firmwareVersion = Regex("MESHMM_VERSION \"([^\"]+)\"")
    .find(repo.resolve("include/Version.h").readText())!!.groupValues[1]
val webAssets = layout.buildDirectory.dir("generated/webAssets")
val copyWeb by tasks.registering(Copy::class) {
    from(repo.resolve("web/index.html"))
    into(webAssets.map { it.dir("web") })
}

android {
    namespace = "org.meshmesh.app"
    compileSdk = 35

    defaultConfig {
        applicationId = "org.meshmesh.app"
        minSdk = 29
        targetSdk = 35
        versionCode = 2
        versionName = "$firmwareVersion-app1"
        buildConfigField("String", "FIRMWARE", "\"$firmwareVersion\"")
    }
    buildFeatures { buildConfig = true }
    sourceSets["main"].assets.srcDir(webAssets)
    // The published APK is signed with the project key (MM_KEYSTORE, MM_KEYSTORE_PASSWORD) so that
    // it updates in place; local builds fall back to the debug key and install as is.
    val keystore = System.getenv("MM_KEYSTORE")?.let(::file)
    signingConfigs {
        if (keystore != null) create("release") {
            storeFile = keystore
            storePassword = System.getenv("MM_KEYSTORE_PASSWORD")
            keyAlias = "meshmesh"
            keyPassword = System.getenv("MM_KEYSTORE_PASSWORD")
        }
    }
    buildTypes {
        release {
            isMinifyEnabled = false
            signingConfig = signingConfigs.getByName(if (keystore != null) "release" else "debug")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions { jvmTarget = "17" }
    testOptions { unitTests.isReturnDefaultValues = true }
}

tasks.named("preBuild") { dependsOn(copyWeb) }

dependencies {
    implementation("androidx.core:core-ktx:1.13.1")
    implementation("androidx.activity:activity-ktx:1.9.3")
    implementation("androidx.webkit:webkit:1.12.1")
    implementation("org.jetbrains.kotlinx:kotlinx-coroutines-android:1.9.0")
    implementation("com.github.mik3y:usb-serial-for-android:3.8.1")
    testImplementation("junit:junit:4.13.2")
    testImplementation("org.json:json:20240303")
    testImplementation("org.jetbrains.kotlinx:kotlinx-coroutines-test:1.9.0")
}
