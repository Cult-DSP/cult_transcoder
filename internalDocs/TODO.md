# now

- tackle memory and streaming tasks
- clean up api and cli - and its documentation
- do some testing
- deprecate parity and assess what other tests are necessary
- assess final steps before considering module complete

# after

- deeper authoring xml streaming:
  - keep one compatibility xml string for bw64 `axml` / metadata-post-data, but remove any remaining avoidable duplicate materialization around it
  - investigate whether `bw64::AxmlChunk` / post-data rewrite can support a no-full-xml-string end-state
