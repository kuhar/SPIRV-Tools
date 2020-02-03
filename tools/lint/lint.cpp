// Copyright (c) 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cassert>
#include <cstring>
#include <fstream>
#include <sstream>
#include <vector>

#include "source/opt/log.h"
#include "source/util/string_utils.h"
#include "spirv-tools/libspirv.hpp"
#include "tools/io.h"
#include "tools/util/cli_consumer.h"

enum class LinterChecks {
  DERIVATIVES,
  UNDEFS,
  ALL,
};

class Linter {
 public:
  void scheduleChecks(const std::vector<LinterChecks>&) {}
};

namespace {
// Message consumer for this tool.  Used to emit diagnostics during
// initialization and setup. Note that |source| and |position| are irrelevant
// here because we are still not processing a SPIR-V input file.
void opt_diagnostic(spv_message_level_t level, const char* /*source*/,
                    const spv_position_t& /*positon*/, const char* message) {
  if (level == SPV_MSG_ERROR) {
    fprintf(stderr, "error: ");
  }
  fprintf(stderr, "%s\n", message);
}

void PrintUsage(const char* program) {
  // NOTE: Please maintain flags in lexicographical order.
  printf(
      R"(%s - Lint a SPIR-V binary file.

USAGE: %s [options] [<input>]

The SPIR-V binary is read from <input>. If no file is specified,
or if <input> is "-", then the binary is read from standard input.

NOTE: The linter is experimental.

Options (in lexicographical order):)",
      program, program);
  printf(R"(
  --check-all
               Run all linter checks.)");
  printf(R"(
  --check-derivatives
               Check implicit derivatives.)");
  printf(R"(
  --check-undefs
               Check uses of undefined values.)");
  printf(R"(
  -h, --help
               Print this help.)");
  printf(R"(
  --version
               Display linter version information.
)");
}

// Parses command-line flags. |argc| contains the number of command-line flags.
// |argv| points to an array of strings holding the flags. |linter| is the
// Linter instance used to check the program.
//
// The return value indicates whether the linter should run or not. The linter
// does not need if parsing fails or a terminal flag is found (e.g., help,
// version).
bool ParseFlags(int argc, const char** argv, Linter* linter,
                const char** in_file) {
  std::vector<LinterChecks> linter_checks;
  *in_file = nullptr;

  for (int argi = 1; argi < argc; ++argi) {
    const char* cur_arg = argv[argi];
    if ('-' == cur_arg[0]) {
      if (0 == strcmp(cur_arg, "--version")) {
        spvtools::Logf(opt_diagnostic, SPV_MSG_INFO, nullptr, {}, "%s\n",
                       spvSoftwareVersionDetailsString());
        return false;
      } else if (0 == strcmp(cur_arg, "--help") || 0 == strcmp(cur_arg, "-h")) {
        PrintUsage(argv[0]);
        return false;
      } else if ('\0' == cur_arg[1]) {
        // Setting a filename of "-" to indicate stdin.
        if (!*in_file) {
          *in_file = cur_arg;
        } else {
          spvtools::Error(opt_diagnostic, nullptr, {},
                          "More than one input file specified");
          return false;
        }
      } else if (0 == strcmp(cur_arg, "--check-derivatives")) {
        linter_checks.push_back(LinterChecks::DERIVATIVES);
      } else if (0 == strcmp(cur_arg, "--check-undefs")) {
        linter_checks.push_back(LinterChecks::UNDEFS);
      } else if (0 == strcmp(cur_arg, "--check-all")) {
        linter_checks.push_back(LinterChecks::ALL);
      }
    } else {
      if (!*in_file) {
        *in_file = cur_arg;
      } else {
        spvtools::Error(opt_diagnostic, nullptr, {},
                        "More than one input file specified");
        return false;
      }
    }
  }

  linter->scheduleChecks(linter_checks);
  return true;
}

}  // namespace

int main(int argc, const char** argv) {
  const char* in_file = nullptr;
  Linter linter;
  if (!ParseFlags(argc, argv, &linter, &in_file)) {
    return 2;
  }

  std::vector<uint32_t> binary;
  if (!ReadFile<uint32_t>(in_file, "rb", &binary)) {
    return 1;
  }

  return 0;
}
