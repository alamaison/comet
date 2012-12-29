Comet: a modern C++ language binding for COM
==========

> **IMPORTANT**: You do **not** have to build this library before
> using it.  It is a header-only library.  Many modern C++ libraries
> are developed this way and it makes them much easier to reuse -
> something we _really_ want to encourage.

What is Comet?
--------------

Comet is a library that provides a modern (STL-like) C++ interface
around the complexities of Microsoft COM. It allows you to do both COM
client and COM server programming, without any dependency on either
ATL or MFC.  Instead, it builds upon the C++ Standard Library that is
included with every modern C++ compiler, regardless of platform.

### I have seen other versions of comet. What version is this?

Other versions of Comet are most likely version 1 gamma 32 plus or
minus a few bugfixes.  This is Comet version 2 because it breaks
backward compatibility with those versions so that we can develop it
more freely.

Comet was designed and implemented by [Sofus Mortensen] with
contributions, ideas and bug fixes from [Paul Hollingsworth], Michael
Geddes, [Mikael Lindgren], [Kyle Alons] and [Vladimir Voinkov].  This
version of comet is maintained by [Alexander Lamaison].

How do I use comet?
-------------------

Just add `comet/include` to your compiler's include path and you are
ready to go.  The library is documented inline and you can use
[Doxygen] to generate documentation for it if you want.

Also, read the following articles.  They are based on Comet v1 but, as
yet, there isn't much difference.

* [Introducing Comet](http://www.codeproject.com/Articles/5748/Introducing-Comet)
* [Introduction to Comet](http://www.lambdasoft.dk/comet/introduction/index.htm)

And last but not least, the [Comet version 1 documentation](http://www.lambdasoft.dk/comet/documentation.htm).

Are there any dependencies?
---------------------------

No, not really.  We use [Boost], but only for the test suite so you
don't even need that.

[Sofus Mortensen]: http://www.lambdasoft.dk/sofus/index.html
[Paul Hollingsworth]: http://paulhollingsworth.com/
[Mikael Lindgren]: mailto:mikael.lindgren@chaos.se
[Kyle Alons]: mailto:kalons@yahoo.com
[Vladimir Voinkov]: mailto:voinkovv@mail.ru
[Alexander Lamaison]: https://github.com/alamaison
[Boost]: http://www.boost.org
[Doxygen]: http://www.doxygen.org/

Licensing
---------

Copyright © 2000-2012 Sofus Mortensen and others

This material is provided "as is", with absolutely no warranty
expressed or implied. Any use is at your own risk. Permission to use
or copy this software for any purpose is hereby granted without fee,
provided the above notices are retained on all copies.  Permission to
modify the code and to distribute modified code is granted, provided
the above notices are retained, and a notice that the code was
modified is included with the above copyright notice.
