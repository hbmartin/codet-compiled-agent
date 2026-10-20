void native_commands(const std::string &text) {
  auto renamed = [&] {
    // ruleid: codet-no-command-processor
    std::system(text.c_str());
  };
  if (text.empty()) return;
  // ruleid: codet-no-command-processor
  ::popen(text.c_str(), "r");
  // ok: codet-no-command-processor
  processes.start(spec);
}
