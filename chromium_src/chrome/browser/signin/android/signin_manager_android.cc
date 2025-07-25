/* Copyright (c) 2019 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "chrome/browser/signin/android/signin_manager_android.h"

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "components/signin/public/identity_manager/identity_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "brave/build/android/jni_headers/BraveSigninManager_jni.h"
#include "chrome/android/chrome_jni_headers/SigninManagerImpl_jni.h"

namespace {
// For preventing "unused Java_SigninManager_create method" compile error.
class UnusedClass {
 private:
  void test() {
    Java_SigninManagerImpl_create(nullptr, 0ll, nullptr, nullptr, nullptr);
    Java_SigninManagerImpl_destroy(nullptr, 0ll);
  }
};
}  // namespace

#define Java_SigninManagerImpl_create Java_BraveSigninManager_create
#define Java_SigninManagerImpl_destroy Java_BraveSigninManager_destroy
#include <chrome/browser/signin/android/signin_manager_android.cc>
#undef Java_SigninManagerImpl_create
#undef Java_SigninManagerImpl_destroy
