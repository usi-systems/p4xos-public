#!/usr/bin/env python

from twisted.internet import reactor
from twisted.internet.task import deferLater
from twisted.web.resource import Resource
from twisted.web.server import Site, NOT_DONE_YET

class MainPage(Resource):
  def getChild(self, name, request):
    if name == '':
      return self
    else:
      return Resource.getChild(self, name, request)

  def render_GET(self, request):
    f = open('../web/index.html', 'r')
    return f.read()  

class PaxosServer(Resource):
  isLeaf = True

  def __init__(self):
    Resource.__init__(self)
    self.db = {}

  def _delayedGet(self, request):
    k = request.args['key'][0].strip()
    try:
      v = self.db[k]
      request.write("%s" % (v,))
      request.finish()
    except KeyError:
      request.write("size %d. Key %s not found" % (len(self.db), k,))
      request.finish()


  def _delayedPut(self, request):
    k = request.args['key'][0].strip()
    v = request.args['value'][0].strip()
    self.db[k] = v
    for x in self.db.keys():
      request.write("(%s, %s)\t" % (x, self.db.get(x)))
    request.write("Stored successfully")
    request.finish()

  def render_GET(self, request):
    d = deferLater(reactor, 1, lambda: request)
    d.addCallback(self._delayedGet)
    return NOT_DONE_YET

  def render_POST(self, request):
    d = deferLater(reactor, 0.001, lambda: request)
    d.addCallback(self._delayedPut)
    return NOT_DONE_YET

if __name__=='__main__':
    root = MainPage()
    server = PaxosServer()
    root.putChild('get', server)
    root.putChild('put', server)
    factory = Site(root)
    reactor.listenTCP(8080, factory)
    reactor.run()
