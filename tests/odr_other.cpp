#include <uipc/uipc.hpp>
unsigned other_request_id() { return uipc::request::new_request_id(); }
