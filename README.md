<img src="screenshot.png" />

# Mayo-Workbench

Small CRUD application to inspect, modify, sort, reason and swear about data
stored in [OlegDB.](https://olegdb.org/)

## Installation

Requires two dependencies:

* [38-Moths](https://github.com/qpfiffer/38-Moths)
* [liboleg-http](https://github.com/qpfiffer/oleg/tree/master/c)

```
make
```

Theres no `make install`. Deal with it. Just run the binary from wherever you
built it, damn it.

## Usage

```
./mayoworkbench -n <dbname> [-h db-hostname/IP] [-p db-port]
```

You'll need to specify the name of the database you want to connect to. Once
it's running, you can point your web-browser at `http://localhost:8666/` to get
started.

It's pretty minimal right now, so do whatever.

