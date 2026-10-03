#include <uipc/uipc.hpp>
unsigned other_request_id();
int main() { return other_request_id() == uipc::request::new_request_id(); }
