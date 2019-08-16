#if os(macOS)
func test(c: /*C:ref:swift*/C) {
  c . /*C.method:call:swift*/method()
}
#endif

func test() {
  /*bridgingHeader:call*/bridgingHeader()
}
