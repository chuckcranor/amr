//
// Created by Ankush J on 4/17/23.
//

#pragma once

#include <pdlfs-common/env.h>
#include <regex>
#include <string>

namespace amr {
class Utils {
 public:
  static int EnsureDir(pdlfs::Env* env, const std::string& dir_path) {
    pdlfs::Status s = env->CreateDir(dir_path.c_str());
    if (s.ok()) {
      logf(LOG_INFO, "\t- Created successfully.");
    } else if (s.IsAlreadyExists()) {
      logf(LOG_INFO, "\t- Already exists.");
    } else {
      logf(LOG_ERRO, "Failed to create output directory: %s (Reason: %s)",
           dir_path.c_str(), s.ToString().c_str());
      return -1;
    }

    return 0;
  }

  static std::vector<std::string> FilterByRegex(
      std::vector<std::string>& strings, std::string regex_pattern) {
    std::vector<std::string> matches;
    const std::regex regex_obj(regex_pattern);

    for (auto& s : strings) {
      std::smatch match_obj;
      if (std::regex_match(s, match_obj, regex_obj)) {
        matches.push_back(s);
      }
    }
    return matches;
  }

  static std::vector<std::string> LocateTraceFiles(
      pdlfs::Env* env, const std::string& search_dir) {
    logf(LOG_INFO, "[SimulateTrace] Looking for trace files in: \n\t%s",
         search_dir.c_str());

    std::vector<std::string> all_files;
    env->GetChildren(search_dir.c_str(), &all_files);

    logf(LOG_DBG2, "Enumerating directory: %s", search_dir.c_str());
    for (auto& f : all_files) {
      logf(LOG_DBG2, "- File: %s", f.c_str());
    }

    std::vector<std::string> regex_patterns = {
        R"(prof\.merged\.evt\d+\.csv)",
        R"(prof\.merged\.evt\d+\.mini\.csv)",
        R"(prof\.aggr\.evt\d+\.csv)",
    };

    std::vector<std::string> relevant_files;
    for (auto& pattern : regex_patterns) {
      logf(LOG_DBG2, "Searching by pattern: %s", pattern.c_str());
      relevant_files = FilterByRegex(all_files, pattern);

      for (auto& f : relevant_files) {
        logf(LOG_DBG2, "- Match: %s", f.c_str());
      }

      if (!relevant_files.empty()) break;
    }

    if (relevant_files.empty()) {
      ABORT("no trace files found!");
    }

    std::vector<std::string> all_fpaths;

    for (auto& f : relevant_files) {
      std::string full_path = std::string(search_dir) + "/" + f;
      logf(LOG_INFO, "[ProfSetReader] Adding trace file: %s",
           full_path.c_str());
      all_fpaths.push_back(full_path);
    }

    return all_fpaths;
  }
};
}  // namespace amr