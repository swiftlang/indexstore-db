func /*asyncFunc:def*/asyncFunc() async {}

struct MyStruct {
  func /*asyncMethod:def*/asyncMethod() async {}
}

class XCTestCase {}
class MyTestCase : XCTestCase {
  func /*testMe:def*/testMe() {}
  func /*testMeAsync:def*/testMeAsync() async {}
}
