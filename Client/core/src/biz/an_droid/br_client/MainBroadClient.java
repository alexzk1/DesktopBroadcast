package biz.an_droid.br_client;

import biz.an_droid.br_client.network.ClientConnector;
import biz.an_droid.br_client.network.IFrameCallback;
import biz.an_droid.br_client.proto.broadcast;
import com.badlogic.gdx.ApplicationAdapter;
import com.badlogic.gdx.Gdx;
import com.badlogic.gdx.InputProcessor;
import com.badlogic.gdx.graphics.GL20;
import com.badlogic.gdx.graphics.Pixmap;
import com.badlogic.gdx.graphics.Texture;
import com.badlogic.gdx.graphics.g2d.SpriteBatch;

import java.io.IOException;

public class MainBroadClient extends ApplicationAdapter
{
    ClientConnector conn = null;
    SpriteBatch batch;

    Texture lastTexture = null;
    Pixmap  pixmap=  null;
    final Object lock = new Object();

    @Override
    public void create()
    {
        batch = new SpriteBatch();
        Gdx.input.setInputProcessor(input);
    }

    @Override
    public void render()
    {
        Gdx.gl.glClearColor(0, 0, 0, 1);
        Gdx.gl.glClear(GL20.GL_COLOR_BUFFER_BIT);
        batch.begin();
        synchronized (lock)
        {
            deleteTexture();
            if (pixmap != null)
                lastTexture = new Texture(pixmap);

            if (lastTexture != null)
                batch.draw(lastTexture, 0, 0);
        }
        batch.end();
    }

    private void deleteTexture()
    {
        if (lastTexture != null)
        {
            lastTexture.dispose();
            lastTexture = null;
        }
    }

    private void deletePixmap()
    {
        if (pixmap != null)
        {
            pixmap.dispose();
            pixmap = null;
        }
    }

    @Override
    public void dispose()
    {
        disconnect();
        synchronized (lock)
        {
            batch.dispose();
            deleteTexture();
            deletePixmap();
        }
    }

    private void connect() throws IOException
    {
        broadcast.Request.connect info = new broadcast.Request.connect();
        info.win_caption = "Creator";
        info.screen_width  = Gdx.graphics.getWidth();
        info.screen_height = Gdx.graphics.getHeight();
        conn = new ClientConnector("192.168.0.100", info, fcb);
    }

    private void disconnect()
    {
        if (conn != null)
        {
            conn.stop();
            conn = null;
        }
    }

    final IFrameCallback fcb = new IFrameCallback()
    {
        @Override
        public void onFrameCame(broadcast.Reply.frame frame)
        {
            if ((frame.flags & 2) != 0)
            {
                synchronized (lock)
                {
                    deletePixmap();
                    pixmap = new Pixmap(frame.data, 0, frame.data.length);
                }
            }
        }
    };

    final InputProcessor input = new InputProcessor()
    {
        @Override
        public boolean keyDown(int keycode)
        {
            return false;
        }

        @Override
        public boolean keyUp(int keycode)
        {
            return false;
        }

        @Override
        public boolean keyTyped(char character)
        {
            return false;
        }

        @Override
        public boolean touchDown(int screenX, int screenY, int pointer, int button)
        {
            return false;
        }

        @Override
        public boolean touchUp(int screenX, int screenY, int pointer, int button)
        {
            if (conn == null || conn.isStopped())
            {
                try
                {
                    connect();
                } catch (IOException e)
                {
                    e.printStackTrace();
                }
            }
            else
                disconnect();

            return true;
        }

        @Override
        public boolean touchDragged(int screenX, int screenY, int pointer)
        {
            return false;
        }

        @Override
        public boolean mouseMoved(int screenX, int screenY)
        {
            return false;
        }

        @Override
        public boolean scrolled(int amount)
        {
            return false;
        }
    };

}
