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
