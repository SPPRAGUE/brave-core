// Copyright (c) 2025 The Brave Authors. All rights reserved.
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at https://mozilla.org/MPL/2.0/.

#include "brave/components/psst/browser/core/rule_data_reader.h"

#include <optional>

#include "base/files/file_util.h"

namespace psst {

namespace {

const base::FilePath::CharType kScriptsDir[] = FILE_PATH_LITERAL("scripts");

std::optional<std::string> ReadFile(const base::FilePath& file_path) {
  std::string contents;
  if (bool success = base::ReadFileToString(file_path, &contents);
      !success || contents.empty()) {
    return std::nullopt;
  }
  return contents;
}

}  // namespace

RuleDataReader::RuleDataReader(const base::FilePath& component_path)
    : prefix_(base::FilePath(component_path).Append(kScriptsDir)) {}

std::optional<std::string> RuleDataReader::ReadUserScript(
    const std::string& rule_name,
    const base::FilePath& user_script_path) const {
  return ReadFile(
      base::FilePath(prefix_).AppendASCII(rule_name).Append(user_script_path));
}

std::optional<std::string> RuleDataReader::ReadPolicyScript(
    const std::string& rule_name,
    const base::FilePath& policy_script_path) const {
  return ReadFile(base::FilePath(prefix_).AppendASCII(rule_name).Append(
      policy_script_path));
}

}  // namespace psst
