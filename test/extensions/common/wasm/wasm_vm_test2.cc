#include "envoy/registry/registry.h"

#include "common/stats/isolated_store_impl.h"

#include "extensions/common/wasm/wasm_vm.h"
#include "extensions/common/wasm/well_known_names.h"

#include "test/test_common/environment.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "include/proxy-wasm/null_vm_plugin.h"

using proxy_wasm::WasmCallVoid; // NOLINT
using proxy_wasm::WasmCallWord; // NOLINT
using proxy_wasm::Word;         // NOLINT
using testing::Return;          // NOLINT

namespace Envoy {
namespace Extensions {
namespace Common {
namespace Wasm {
namespace {


class WasmHelloWorldVmTest : public testing::TestWithParam<std::string> {
public:
  WasmHelloWorldVmTest() : scope_(Stats::ScopeSharedPtr(stats_store.createScope("wasm."))) {}

protected:
  Stats::IsolatedStoreImpl stats_store;
  Stats::ScopeSharedPtr scope_;
};

auto test_suite_parameters = testing::Values(
#if defined(ENVOY_WASM_V8)
    WasmRuntimeNames::get().V8
#endif
#if defined(ENVOY_WASM_V8) && defined(ENVOY_WASM_WAVM)
    ,
#endif
#if defined(ENVOY_WASM_WAVM)
    WasmRuntimeNames::get().Wavm
#endif
);

INSTANTIATE_TEST_SUITE_P(Runtime, WasmHelloWorldVmTest, test_suite_parameters);

TEST_P(WasmHelloWorldVmTest, HelloWorld) {
  auto wasm_vm = createWasmVm(GetParam(), scope_);
  ASSERT_TRUE(wasm_vm != nullptr);

  auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/common/wasm/test_data/test_rust.wasm"));
  EXPECT_TRUE(wasm_vm->load(code, true));

  EXPECT_TRUE(wasm_vm->link("test"));

  WasmCallWord<3> sum;
  wasm_vm->getFunction("sum", &sum);
  EXPECT_EQ(42, sum(nullptr /* no context */, 13, 14, 15).u64_);

}


} // namespace
} // namespace Wasm
} // namespace Common
} // namespace Extensions
} // namespace Envoy 
