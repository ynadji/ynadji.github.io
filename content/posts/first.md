+++
title = "Python Simple One-hot Encoding"
tags = [
     "python",
     "tricks",
     "ml",
]
categories = [
]
date = "2019-10-31"
+++

## One-hot Encoding

_One-hot encoding_ transforms categorical data into a fixed-size numeric vector,
where each index maps to one of the unique categories present in the input
data. While you can use `sklearn.preprocessing.OneHotEncoder` or
`pandas.get_dummies` to perform this transformation for you, sometimes it's nice
to be able to do this just with the Python standard library for quick'n'dirty
scripts. Using Python's built-in `defaultdict` data structure and `itertools`
package, we can make a "dictionary-like" data structure that maps any hashable
data to a unique integer.

## Code
```python
import itertools
from collections import defaultdict

onehot = defaultdict(itertools.count().__next__)
```
