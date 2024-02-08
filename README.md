# Asynchronous Rust Runtimes via FFI

A demo showing how to use an asynchronous Rust library from other
languages via FFI. Among other things, this requires the implementation
of an async Rust runtime in the target language.

## Premises

- External I/O, i.e. Networking, provided by the library consumer

  - no I/O in library itself

- Usage on mobile clients (Cordova-based), servers and embedded
- Pull-only data access

  This was a constraint from the real project.
  Also, it precludes the need for circular references between the client,
  the library and the transport layer.
