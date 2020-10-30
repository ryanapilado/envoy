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


class MockHostFunctions {
public:
  MOCK_METHOD(void, pong, (uint32_t), (const));
  MOCK_METHOD(uint32_t, random, (), (const));
};

MockHostFunctions* g_host_functions;
void pong(void*, Word value) { g_host_functions->pong(convertWordToUint32(value)); }
Word random(void*) { return {g_host_functions->random()}; }


class WasmAbiRestrictionTest : public testing::TestWithParam<std::string> {
public:
  WasmAbiRestrictionTest() : scope_(Stats::ScopeSharedPtr(stats_store.createScope("wasm."))) {}

  void SetUp() override { // NOLINT(readability-identifier-naming)
    g_host_functions = new MockHostFunctions();
  }
  void TearDown() override { delete g_host_functions; }

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

INSTANTIATE_TEST_SUITE_P(Runtime, WasmAbiRestrictionTest, test_suite_parameters);

TEST_P(WasmAbiRestrictionTest, ExposeABI) {
  auto wasm_vm = createWasmVm(GetParam(), scope_);
  ASSERT_TRUE(wasm_vm != nullptr);

  auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/common/wasm/test_data/test_rust.wasm"));
  EXPECT_TRUE(wasm_vm->load(code, true));

  // All imports/exports exposed
  wasm_vm->restrictABI();
  wasm_vm->exposeFunction("pong");
  wasm_vm->exposeFunction("random");
  wasm_vm->exposeFunction("ping");
  wasm_vm->exposeFunction("lucky");
  wasm_vm->exposeFunction("sum");
  wasm_vm->exposeFunction("div");
  wasm_vm->exposeFunction("abort");

  wasm_vm->registerCallbackIfExposed("env", "pong", &pong, CONVERT_FUNCTION_WORD_TO_UINT32(pong));
  wasm_vm->registerCallbackIfExposed(
    "env", "random", &random, CONVERT_FUNCTION_WORD_TO_UINT32(random));
  EXPECT_TRUE(wasm_vm->link("test"));

  WasmCallVoid<1> ping;
  wasm_vm->getFunctionIfExposed("ping", &ping);
  EXPECT_CALL(*g_host_functions, pong(42));
  ping(nullptr /* no context */, 42);

  WasmCallWord<1> lucky;
  wasm_vm->getFunctionIfExposed("lucky", &lucky);
  EXPECT_CALL(*g_host_functions, random()).WillRepeatedly(Return(42));
  EXPECT_EQ(0, lucky(nullptr /* no context */, 1).u64_);
  EXPECT_EQ(1, lucky(nullptr /* no context */, 42).u64_);

  WasmCallWord<3> sum;
  wasm_vm->getFunctionIfExposed("sum", &sum);
  EXPECT_EQ(42, sum(nullptr /* no context */, 13, 14, 15).u64_);

  WasmCallWord<2> div;
  wasm_vm->getFunctionIfExposed("div", &div);
  EXPECT_EQ(1, (div(nullptr /* no context */, 42, 42)).u64_);
  div(nullptr /* no context */, 42, 0);
  EXPECT_TRUE(wasm_vm->isFailed());

  WasmCallVoid<0> abort;
  wasm_vm->getFunctionIfExposed("abort", &abort);
  abort(nullptr /* no context */);
  EXPECT_TRUE(wasm_vm->isFailed());

}

TEST_P(WasmAbiRestrictionTest, V8RestrictHostFunction) {
  auto wasm_vm = createWasmVm(GetParam(), scope_);
  ASSERT_TRUE(wasm_vm != nullptr);

  auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/common/wasm/test_data/test_rust.wasm"));
  EXPECT_TRUE(wasm_vm->load(code, true));

  WasmCallVoid<1> ping;

  wasm_vm->restrictABI();
  // host functions not exposed
  wasm_vm->exposeFunction("random");
  wasm_vm->exposeFunction("ping");
  wasm_vm->exposeFunction("lucky");
  wasm_vm->exposeFunction("sum");
  wasm_vm->exposeFunction("div");
  wasm_vm->exposeFunction("abort");

  wasm_vm->registerCallbackIfExposed("env", "pong", &pong, CONVERT_FUNCTION_WORD_TO_UINT32(pong));
  wasm_vm->registerCallbackIfExposed(
    "env", "random", &pong, CONVERT_FUNCTION_WORD_TO_UINT32(pong));
  EXPECT_FALSE(wasm_vm->link("test"));
  EXPECT_TRUE(wasm_vm->isFailed());

}

TEST_P(WasmAbiRestrictionTest, V8RestrictModuleFunction) {
  auto wasm_vm = createWasmVm(GetParam(), scope_);
  ASSERT_TRUE(wasm_vm != nullptr);

  auto code = TestEnvironment::readFileToStringForTest(TestEnvironment::substitute(
      "{{ test_rundir }}/test/extensions/common/wasm/test_data/test_rust.wasm"));
  EXPECT_TRUE(wasm_vm->load(code, true));

  wasm_vm->restrictABI();
  wasm_vm->exposeFunction("pong");
  wasm_vm->exposeFunction("random");
  // module functions (ping, lucky, random, etc.) not exposed

  wasm_vm->registerCallbackIfExposed("env", "pong", &pong, CONVERT_FUNCTION_WORD_TO_UINT32(pong));
  wasm_vm->registerCallbackIfExposed(
    "env", "random", &random, CONVERT_FUNCTION_WORD_TO_UINT32(random));
  EXPECT_FALSE(wasm_vm->link("test"));
  EXPECT_TRUE(wasm_vm->isFailed());

  WasmCallWord<3> sum;
  wasm_vm->getFunctionIfExposed("sum", &sum);
  EXPECT_TRUE(sum == nullptr);

  WasmCallVoid<1> ping;
  wasm_vm->getFunctionIfExposed("ping", &ping);
  EXPECT_TRUE(ping == nullptr);

  WasmCallWord<1> lucky;
  wasm_vm->getFunctionIfExposed("lucky", &lucky);
  EXPECT_TRUE(lucky == nullptr);

  WasmCallWord<2> div;
  wasm_vm->getFunctionIfExposed("div", &div);
  EXPECT_TRUE(div == nullptr);

  WasmCallVoid<0> abort;
  wasm_vm->getFunctionIfExposed("abort", &abort);
  EXPECT_TRUE(abort == nullptr);
}

} // namespace
} // namespace Wasm
} // namespace Common
} // namespace Extensions
} // namespace Envoy