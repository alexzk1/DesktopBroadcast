package biz.an_droid.desktopbroadcast.network;

import biz.an_droid.desktopbroadcast.proto.broadcast;

public interface IFrameCallback
{
    void onFrameCame(broadcast.Reply.frame frame);
}
