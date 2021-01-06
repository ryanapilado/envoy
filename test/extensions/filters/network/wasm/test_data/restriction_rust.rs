use log::trace;
use proxy_wasm::hostcalls;
use proxy_wasm::traits::{Context, StreamContext};
use proxy_wasm::types::*;

#[no_mangle]
pub fn _start() {
    proxy_wasm::set_log_level(LogLevel::Trace);
    proxy_wasm::set_stream_context(|context_id, _| -> Box<dyn StreamContext> {
        Box::new(TestStream { context_id })
    });
}

struct TestStream {
    context_id: u32,
}

impl Context for TestStream {}

impl StreamContext for TestStream {
    fn on_new_connection(&mut self) -> Action {
        hostcalls::proxy_log("onNewConnection");
        Action::Continue
    }

    fn on_downstream_close(&mut self, peer_type: PeerType) {
        hostcalls::proxy_log("onDownstreamConnectionClose");
    }
}