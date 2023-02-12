template <typename T>
concept /*LargeType:def*/LargeType = sizeof(T) > 8;

template </*LargeType:ref*/LargeType T>
void conceptUse();
