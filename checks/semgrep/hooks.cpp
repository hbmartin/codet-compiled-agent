void hooks() {
  // ruleid: codet-hook-stop-monotonic
  out.stopped = stop;
  // ruleid: codet-hook-stop-monotonic
  out.stopped = false;
  // ok: codet-hook-stop-monotonic
  out.stopped = true;
  // ok: codet-hook-stop-monotonic
  out.stopped = out.stopped || stop;
}

void renamed_and_nested_hooks(bool later_veto) {
  auto apply = [&] {
    // ruleid: codet-hook-stop-monotonic
    renamed.stopped = later_veto;
    // ruleid: codet-hook-stop-monotonic
    renamed.stopped = false;
    // ok: codet-hook-stop-monotonic
    renamed.stopped = renamed.stopped || later_veto;
    // ok: codet-hook-stop-monotonic
    renamed.stopped = true;
  };
}
