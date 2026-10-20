void native_commands(const char *text) {
  // ruleid: codet-no-command-processor
  system(text);
  // ruleid: codet-no-command-processor
  popen(text, "r");
  // ok: codet-no-command-processor
  processes_start(text);
}
