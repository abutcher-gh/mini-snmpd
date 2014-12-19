void install_pass_persist_handler(const char* oid, const char* command);
struct oid_s;
struct value_s *maybe_handle_pass_persist(const char* method, const struct oid_s *oid);
