package biz.an_droid.br_client.network;
import biz.an_droid.br_client.proto.broadcast;

public interface IFrameCallback
{
    void onFrameCame(broadcast.Reply.frame frame);
}
