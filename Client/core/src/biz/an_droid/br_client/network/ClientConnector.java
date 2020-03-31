package biz.an_droid.br_client.network;

import biz.an_droid.br_client.proto.broadcast;

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
    private final AtomicBoolean stopped = new AtomicBoolean(true);
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
        stopped.set(false);
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

    public final boolean isStopped()
    {
        return stopped.get();
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
                while (!needStop.get() && socket.isConnected())
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
            stopped.set(true);
        }
    };
}
