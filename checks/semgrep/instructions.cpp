// Semgrep fixtures, intentionally uncompiled. Cover constructors and status queries.
void discovery(fs::path dir, fs::directory_entry entry) {
  // ruleid: codet-discovery-throwing-filesystem
  for (const auto &child : fs::directory_iterator(dir)) {}
  // ruleid: codet-discovery-throwing-filesystem
  fs::directory_iterator bad(dir);
  // ruleid: codet-discovery-throwing-filesystem
  entry.is_symlink();
  // ruleid: codet-discovery-throwing-filesystem
  entry.is_directory();
  // ruleid: codet-discovery-throwing-filesystem
  entry.is_regular_file();
  // ruleid: codet-discovery-throwing-filesystem
  fs::exists(dir);
  // ruleid: codet-discovery-throwing-filesystem
  fs::is_symlink(dir);
  // ruleid: codet-discovery-throwing-filesystem
  fs::is_directory(dir);
  // ruleid: codet-discovery-throwing-filesystem
  fs::is_regular_file(dir);
  // ruleid: codet-discovery-throwing-filesystem
  fs::status(dir);
  // ruleid: codet-discovery-throwing-filesystem
  fs::symlink_status(dir);
  std::error_code error;
  // ok: codet-discovery-throwing-filesystem
  fs::directory_iterator good(dir, error), end;
  // ok: codet-discovery-throwing-filesystem
  good.increment(error);
  // ok: codet-discovery-throwing-filesystem
  fs::status(dir, error);
  // ok: codet-discovery-throwing-filesystem
  fs::symlink_status(dir, error);
  // ok: codet-discovery-throwing-filesystem
  fs::is_regular_file(dir, error);
  // ok: codet-discovery-throwing-filesystem
  entry.is_directory(error);
}
