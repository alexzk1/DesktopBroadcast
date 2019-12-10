package biz.an_droid.desktopbroadcast.network;

import biz.an_droid.desktopbroadcast.proto.broadcast;

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
                DataInputStream dis = new DataInputStream(socket.getInputStream());
                while (!needStop.get())
                {
                    try
                    {
                        if (dis.available() > 3)
                        {
                            broadcast.Reply r = broadcast.Reply.unmarshal(dis);
                            if (r instanceof broadcast.Reply.frame)
                            {
                                broadcast.Reply.frame fr = (broadcast.Reply.frame) r;
                                callback_frame.onFrameCame(fr);
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
