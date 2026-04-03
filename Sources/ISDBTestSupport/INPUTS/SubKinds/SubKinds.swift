struct Box {
  subscript(/*boxSubscript:param*/index: Int) -> Int {
    /*boxSubscript:get*/get { index }
    set { _ = newValue }
  }
}

func useBox() {
  let box = Box()
  _ = box[/*boxSubscript:call*/0]
}
