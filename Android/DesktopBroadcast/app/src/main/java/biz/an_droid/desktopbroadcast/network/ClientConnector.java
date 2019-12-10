package biz.an_droid.desktopbroadcast.network;

import biz.an_droid.desktopbroadcast.proto.broadcast;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;
import java.util.concurrent.atomic.AtomicBoolean;

public class ClientConnector
{
    private final broadcast.Request.connect info;
    private final int CLIENT_VERSION = 0x01;
    private final AtomicBoolean needStop = new AtomicBoolean(false);
    private final IFrameCallback callback_frame;
    private InetSocketAddress endPoint = null;
    private Thread thr = null;


    public ClientConnector(String address, broadcast.Request.connect info, IFrameCallback cb) throws IOException
    {
        info.version_client = CLIENT_VERSION;
        this.info = info;
        callback_frame = cb;
        connectMethod(address, 11222);
    }

    public ClientConnector(String address, int port, broadcast.Request.connect info, IFrameCallback cb) throws IOException
    {
        info.version_client = CLIENT_VERSION;
        this.info = info;
        callback_frame = cb;
        connectMethod(address, port);
    }

    private void connectMethod(String ip, int port) throws IOException
    {
        endPoint = new InetSocketAddress(ip, port);
        thr = new Thread(exec);
        thr.start();
    }

    public final void stop()
    {
        needStop.set(true);
        try
        {
            thr.join();
        } catch (InterruptedException ignored)
        {
        }
    }

    private final Runnable exec = new Runnable()
    {
        @Override
        public void run()
        {
            try (Socket socket = new Socket())
            {
                socket.connect(endPoint);
                info.marshal(socket.getOutputStream());
                BufferedInputStream bis = new BufferedInputStream(socket.getInputStream());
                DataInputStream dis = new DataInputStream(bis);
                final int limit = 3 * (info.screen_height * info.screen_width * 4 + 100);
                while (!needStop.get())
                {
                    try
                    {
                        if (dis.available() > 0)
                        {
                            dis.mark(limit);
                            try
                            {
                                broadcast.Reply r = broadcast.Reply.unmarshal(dis);
                                try
                                {
                                    if (r instanceof broadcast.Reply.frame)
                                    {
                                        broadcast.Reply.frame fr = (broadcast.Reply.frame) r;
                                        callback_frame.onFrameCame(fr);
                                    }
                                } catch (Exception e)
                                {
                                    e.printStackTrace();
                                }
                            } catch (Exception ignored)
                            {
                                dis.reset();
                                try
                                {
                                    Thread.sleep(50);
                                } catch (InterruptedException e)
                                {
                                    break;
                                }
                            }
                        }

                    } catch (IOException e)
                    {
                        e.printStackTrace();
                    }
                }
            } catch (IOException e)
            {
                e.printStackTrace();
            }
        }
    };
}
