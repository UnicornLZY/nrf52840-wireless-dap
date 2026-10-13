# Contributing

Please open an issue describing the problem or proposed change. Include the board/target model, firmware hash, wiring and power arrangement, host tool versions, complete errors, and reproducible steps. Do not attach passwords, tokens or full-flash snapshots from unrelated projects.

Keep changes focused. Run `run_tests.ps1` and `build.ps1`, and describe what was tested on real hardware separately from the desktop model results. Wireless retransmission must not duplicate target DAP execution; UART changes must retain order, duplicate suppression and backpressure behavior.

Protocol-incompatible changes need a distinct version and require both peers to update. Do not import failed historical worker-priority, larger-window or automatic target-write replay experiments without new evidence.

Project-authored contributions use MIT; preserve all third-party notices. Update docs, validation status and the source manifest when changing the production snapshot. A public candidate is not automatically a hardware-validated stable release.
