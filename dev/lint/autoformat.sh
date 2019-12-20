#!/bin/sh

format() {
  find include src tests tools -type f -exec clang-format -i ${}
}
