using System.Collections.Generic;
using System.Threading.Tasks;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using SixLabors.ImageSharp.Processing;

namespace OpenTibiaUnity
{
    public class AsyncGraphics
    {
        Image<Rgba32> m_Image;
        List<Task> m_DrawTasks = new List<Task>();

        // mutex
        object m_DrawLock = new object();

        public AsyncGraphics(Image<Rgba32> image) {
            m_Image = image ?? throw new System.ArgumentNullException("Invalid image");
        }

        public void DrawImage(Image image, int x, int y) => m_DrawTasks.Add(Task.Run(() => InternalDraw(image, x, y)));
        public void DrawImage(Image image, Point point) => m_DrawTasks.Add(Task.Run(() => InternalDraw(image, point.X, point.Y)));
        public void DrawImage(Image image, int x, int y, int w, int h) => m_DrawTasks.Add(Task.Run(() => InternalDraw(image, x, y, w, h)));
        public void DrawImage(Image image, Rectangle rect) => m_DrawTasks.Add(Task.Run(() => InternalDraw(image, rect.X, rect.Y, rect.Width, rect.Height)));

        public void DrawImages(List<Image> images, List<Point> points) {
            if (images.Count != points.Count)
                throw new System.ArgumentException("Invalid draw call: points.Count != image.Count");

            for (int i = 0; i < images.Count; i++) {
                var image = images[i];
                var point = points[i];

                DrawImage(image, point);
            }
        }

        void InternalDraw(Image image, int x, int y) {
            lock (m_DrawLock) {
                m_Image.Mutate(ctx => ctx.DrawImage(image, new Point(x, y), 1f));
                image.Dispose();
            }
        }

        void InternalDraw(Image image, int x, int y, int w, int h) {
            if (image.Width != w || image.Height != h)
                image.Mutate(ctx => ctx.Resize(w, h));
            InternalDraw(image, x, y);
        }

        public async Task DisposeOnDone(IEnumerable<Image<Rgba32>> images) {
            await Task.WhenAll(m_DrawTasks);
            foreach (var image in images)
                image?.Dispose();
        }

        public async Task SaveAndDispose(string filename) {
            await Task.WhenAll(m_DrawTasks);
            m_Image.Save(filename);
            m_Image.Dispose();
        }
    }
}
