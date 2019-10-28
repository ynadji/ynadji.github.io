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
draft = true
+++

## One-hot Encoding

_One-hot encoding_ transforms categorical data into a fixed-size numeric vector,
where each index maps to one of the unique categories present in the input
data. While you can use `sklearn.preprocessing.OneHotEncoder` or
`pandas.get_dummies` to perform this transformation for you, sometimes it's nice
to be able to do this just with the Python standard library for quick'n'dirty
scripts. Using Python's built-in `defaultdict` data structure and `itertools`
package, we can make a "dictionary-like" data structure that maps any hashable
data to a unique integer. If the key has not been seen before, its value will be
the next unique identifier (starting with `0`), otherwise its index value will be
returned.

## Code
```python
>>> import itertools
>>> from collections import defaultdict

>>> onehot = defaultdict(itertools.count().__next__)
>>> onehot['a']
0
>>> onehot[('b', 'c')]
1
>>> onehot['d']
2
>>> onehot['d']
2
```

A normal `dict` can easily be retrieved with:

```python
>>> dict(onehot)
{'a': 0, ('b', 'c'): 1, 'd': 2}
```

and since there's a one-to-one mapping, you can quickly retrieve the reverse
mapping---of indices to categories---with the following `dict` comprehension:

```python
>>> {v: k for (k, v) in onehot.items()}
{0: 'a', 1: ('b', 'c'), 2: 'd'}
```
