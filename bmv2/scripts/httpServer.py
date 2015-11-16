#!/usr/bin/env python

import os, json, argparse, ConfigParser
from twisted.internet import reactor, defer
from twisted.internet.task import deferLater
from twisted.web.resource import Resource
from twisted.web.server import Site, NOT_DONE_YET
from twisted.web import static

THIS_DIR=os.path.dirname(os.path.realpath(__file__))
from paxoscore.proposer import Proposer

class MainPage(Resource):
  def getChild(self, name, request):
    if name == '':
      return self
    else:
      print name, request
      return Resource.getChild(self, name, request)

  def render_GET(self, request):
    f = open('%s/web/index.html' % THIS_DIR, 'r')
    return f.read()  

class WebServer(Resource):
  isLeaf = True

  def __init__(self, proposer):
    Resource.__init__(self)
    self.proposer = proposer

  def _waitResponse(self, result, request):
    result = result.rstrip('\t\r\n\0')
    request.write(result)
    request.finish()

  def render_GET(self, request):
    print request
    request.args['action'] = 'get'
    data = json.dumps(request.args)
    d = self.proposer.submit(data)
    d.addCallback(self._waitResponse, request)
    return NOT_DONE_YET

  def render_POST(self, request):
    print request
    request.args['action'] = 'put'
    data = json.dumps(request.args)
    d = self.proposer.submit(data)
    d.addCallback(self._waitResponse, request)
    return NOT_DONE_YET

if __name__=='__main__':
    parser = argparse.ArgumentParser(description='Paxos Proposer.')
    parser.add_argument('--cfg', required=True)
    args = parser.parse_args()
    config = ConfigParser.ConfigParser()
    config.read(args.cfg)
    proposer = Proposer(config, 0)
    reactor.listenUDP(config.getint('proposer', 'port'), proposer)

    root = MainPage()
    server = WebServer(proposer)
    root.putChild('jquery.min.js', static.File('%s/web/jquery.min.js' % THIS_DIR))
    root.putChild('get', server)
    root.putChild('put', server)
    factory = Site(root)
    reactor.listenTCP(8080, factory)

    reactor.run()
