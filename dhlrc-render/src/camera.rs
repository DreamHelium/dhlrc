/*! Where the picture is taken from.
 *
 * An orbit camera: it always looks at `target`, and `yaw`, `pitch` and
 * `distance` say where it stands around it. That is the whole interaction the
 * Qt view needs (drag to turn, wheel to come closer), and it is also what a
 * headless render is given to reproduce a view.
 *
 * The arithmetic is a small column-major 4x4 matrix, the same convention a GPU
 * wants, so a `Scene` can later be drawn by one without being rebuilt.
 */

/// A point or direction in 0..1 block space, y up.
pub type Vec3 = [f32; 3];

/// A point in space, away from `b`.
pub fn sub(a: Vec3, b: Vec3) -> Vec3 {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}

/// `a` scaled by `factor`.
pub fn scale(a: Vec3, factor: f32) -> Vec3 {
    [a[0] * factor, a[1] * factor, a[2] * factor]
}

pub fn dot(a: Vec3, b: Vec3) -> f32 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}

pub fn cross(a: Vec3, b: Vec3) -> Vec3 {
    [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]
}

/// The direction of `a`, or `fallback` when it has no length.
pub fn normalize(a: Vec3, fallback: Vec3) -> Vec3 {
    let length = dot(a, a).sqrt();
    if length < 1e-6 {
        fallback
    } else {
        scale(a, 1.0 / length)
    }
}

/// A column-major 4x4 matrix: element `(row, col)` is at `col * 4 + row`.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Mat4(pub [f32; 16]);

impl Mat4 {
    pub const IDENTITY: Mat4 = Mat4([
        1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0,
    ]);

    /// `self` then `other` applied to a column vector: `other * self`.
    pub fn multiply(&self, other: &Mat4) -> Mat4 {
        let mut out = [0.0f32; 16];
        for col in 0..4 {
            for row in 0..4 {
                let mut sum = 0.0;
                for k in 0..4 {
                    sum += other.0[k * 4 + row] * self.0[col * 4 + k];
                }
                out[col * 4 + row] = sum;
            }
        }
        Mat4(out)
    }

    /// `point` to clip space, as `[x, y, z, w]`.
    pub fn project(&self, point: Vec3) -> [f32; 4] {
        let m = &self.0;
        [
            m[0] * point[0] + m[4] * point[1] + m[8] * point[2] + m[12],
            m[1] * point[0] + m[5] * point[1] + m[9] * point[2] + m[13],
            m[2] * point[0] + m[6] * point[1] + m[10] * point[2] + m[14],
            m[3] * point[0] + m[7] * point[1] + m[11] * point[2] + m[15],
        ]
    }
}

/// A right-handed look-at matrix, as OpenGL wants it.
pub fn look_at(eye: Vec3, target: Vec3, up: Vec3) -> Mat4 {
    let forward = normalize(sub(target, eye), [0.0, 0.0, -1.0]);
    let side = normalize(cross(forward, up), [1.0, 0.0, 0.0]);
    let up = cross(side, forward);
    Mat4([
        side[0],
        up[0],
        -forward[0],
        0.0,
        side[1],
        up[1],
        -forward[1],
        0.0,
        side[2],
        up[2],
        -forward[2],
        0.0,
        -dot(side, eye),
        -dot(up, eye),
        dot(forward, eye),
        1.0,
    ])
}

/// A perspective projection with `fov_y` in degrees, clip space z in `-1..1`.
pub fn perspective(fov_y: f32, aspect: f32, near: f32, far: f32) -> Mat4 {
    let tan = (fov_y.to_radians() / 2.0).tan();
    let mut m = [0.0f32; 16];
    m[0] = 1.0 / (aspect * tan);
    m[5] = 1.0 / tan;
    m[10] = (far + near) / (near - far);
    m[11] = -1.0;
    m[14] = 2.0 * far * near / (near - far);
    Mat4(m)
}

/// An orthographic projection: `half_height` is how much of the scene fits
/// above the middle of the picture.
pub fn orthographic(half_height: f32, aspect: f32, near: f32, far: f32) -> Mat4 {
    let half_width = half_height * aspect;
    let mut m = [0.0f32; 16];
    m[0] = 1.0 / half_width;
    m[5] = 1.0 / half_height;
    m[10] = -2.0 / (far - near);
    m[14] = -(far + near) / (far - near);
    m[15] = 1.0;
    Mat4(m)
}

/// An orbit camera.
#[derive(Debug, Clone, PartialEq)]
pub struct Camera {
    /// The point the camera looks at, in block space.
    pub target: Vec3,
    /// Degrees around the world's y axis; 0 looks from the south.
    pub yaw: f32,
    /// Degrees above the horizon; 90 looks straight down.
    pub pitch: f32,
    /// How far the camera stands from the target, in blocks.
    pub distance: f32,
    /// The vertical field of view in degrees.
    pub fov: f32,
    /// Whether to use an orthographic projection (an isometric-looking view).
    pub orthographic: bool,
}

impl Camera {
    /// The default view of a scene: from the south-east, above it.
    pub fn new(target: Vec3, distance: f32) -> Self {
        Self {
            target,
            yaw: 45.0,
            pitch: 30.0,
            distance,
            fov: 45.0,
            orthographic: false,
        }
    }

    /// Where the camera stands.
    pub fn eye(&self) -> Vec3 {
        let yaw = self.yaw.to_radians();
        let pitch = self.pitch.to_radians();
        [
            self.target[0] + self.distance * pitch.cos() * yaw.sin(),
            self.target[1] + self.distance * pitch.sin(),
            self.target[2] + self.distance * pitch.cos() * yaw.cos(),
        ]
    }

    /// The view-projection matrix for a picture of `aspect`, with a scene of
    /// `radius` around the target. The near and far planes are placed so the
    /// whole scene is inside them whatever the distance is.
    pub fn view_projection(&self, aspect: f32, radius: f32) -> Mat4 {
        let eye = self.eye();
        let view = look_at(eye, self.target, [0.0, 1.0, 0.0]);
        let far = self.distance + radius * 4.0 + 16.0;
        let projection = if self.orthographic {
            let half_height = self.distance * (self.fov.to_radians() / 2.0).tan();
            orthographic(half_height.max(0.05), aspect, -far, far)
        } else {
            let near = (self.distance * 0.005).max(0.02);
            perspective(self.fov, aspect, near, far)
        };
        /* The view first, then the projection: `multiply` takes the matrix that
         * is applied first as its receiver. */
        view.multiply(&projection)
    }

    /// Points the camera at a box so the whole of it fits in a picture of
    /// `aspect`, whichever of the two fields of view is the tighter one. This
    /// is what a frontend calls when it first shows a scene.
    ///
    /// The box is measured along the camera's own axes rather than by its
    /// diagonal, so a long, flat row of blocks fills the picture instead of
    /// sitting in the middle of a lot of empty space.
    pub fn fit(&mut self, min: Vec3, max: Vec3, aspect: f32) {
        let center = [
            (min[0] + max[0]) / 2.0,
            (min[1] + max[1]) / 2.0,
            (min[2] + max[2]) / 2.0,
        ];
        self.target = center;

        let yaw = self.yaw.to_radians();
        let pitch = self.pitch.to_radians();
        let eye_direction = [
            pitch.cos() * yaw.sin(),
            pitch.sin(),
            pitch.cos() * yaw.cos(),
        ];
        /* The camera's right and up, from where it stands looking at the box. */
        let right = [yaw.cos(), 0.0, -yaw.sin()];
        let forward = scale(eye_direction, -1.0);
        let up = cross(right, forward);

        let mut half_right = 0.0f32;
        let mut half_up = 0.0f32;
        let mut half_depth = 0.0f32;
        for corner in 0..8 {
            let point = [
                if corner & 1 == 0 { min[0] } else { max[0] } - center[0],
                if corner & 2 == 0 { min[1] } else { max[1] } - center[1],
                if corner & 4 == 0 { min[2] } else { max[2] } - center[2],
            ];
            half_right = half_right.max(dot(point, right).abs());
            half_up = half_up.max(dot(point, up).abs());
            half_depth = half_depth.max(dot(point, forward).abs());
        }

        let vertical = self.fov.to_radians();
        let horizontal = 2.0 * ((vertical / 2.0).tan() * aspect.max(0.05)).atan();
        let distance = half_up / (vertical / 2.0).tan();
        let across = half_right / (horizontal / 2.0).tan();
        self.distance = ((distance.max(across) + half_depth) * 1.05).clamp(0.5, 4096.0);
    }

    /// Turns the camera by `yaw` and `pitch` degrees, keeping it above the
    /// horizon and below the zenith.
    pub fn orbit(&mut self, yaw: f32, pitch: f32) {
        self.yaw = (self.yaw + yaw).rem_euclid(360.0);
        self.pitch = (self.pitch + pitch).clamp(-89.9, 89.9);
    }

    /// Moves the camera `factor` times closer (`< 1`) or further (`> 1`).
    pub fn zoom(&mut self, factor: f32) {
        self.distance = (self.distance * factor).clamp(0.5, 4096.0);
    }

    /// Slides the target across the plane the camera looks at, by a fraction of
    /// the picture's height.
    pub fn pan(&mut self, right: f32, up: f32) {
        let yaw = self.yaw.to_radians();
        let pitch = self.pitch.to_radians();
        /* The camera's own right and up vectors, in the two tangent axes. */
        let right_vector = [yaw.cos(), 0.0, -yaw.sin()];
        let up_vector = [
            -pitch.sin() * yaw.sin(),
            pitch.cos(),
            -pitch.sin() * yaw.cos(),
        ];
        let step = self.distance * (self.fov.to_radians() / 2.0).tan() * 2.0;
        for axis in 0..3 {
            self.target[axis] += (right_vector[axis] * right + up_vector[axis] * up) * step;
        }
    }
}

/// A ray through the scene, in block space.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Ray {
    pub origin: Vec3,
    pub direction: Vec3,
}

/// Where a ray met a block's surface.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Hit {
    /// The block that was hit, in the region's own coordinates.
    pub block: [i32; 3],
    /// How far along the ray the surface is.
    pub distance: f32,
    /// The face that was hit, looking out of the block.
    pub normal: Vec3,
}

impl Camera {
    /// The camera's own axes: to the right, up and forward.
    fn axes(&self) -> (Vec3, Vec3, Vec3) {
        let yaw = self.yaw.to_radians();
        let pitch = self.pitch.to_radians();
        let to_eye = [
            pitch.cos() * yaw.sin(),
            pitch.sin(),
            pitch.cos() * yaw.cos(),
        ];
        let forward = scale(to_eye, -1.0);
        let right = [yaw.cos(), 0.0, -yaw.sin()];
        let up = cross(right, forward);
        (right, up, forward)
    }

    /// The ray through pixel `(x, y)` of a `width` by `height` picture, for
    /// picking the block under the mouse.
    pub fn ray(&self, x: f32, y: f32, width: u32, height: u32) -> Ray {
        let width = width.max(1) as f32;
        let height = height.max(1) as f32;
        let across = (x / width) * 2.0 - 1.0;
        let down = 1.0 - (y / height) * 2.0;
        let (right, up, forward) = self.axes();
        let tan = (self.fov.to_radians() / 2.0).tan();

        if self.orthographic {
            let half_height = self.distance * tan;
            let eye = self.eye();
            let origin = [
                eye[0] + right[0] * across * half_height * width / height
                    + up[0] * down * half_height,
                eye[1] + right[1] * across * half_height * width / height
                    + up[1] * down * half_height,
                eye[2] + right[2] * across * half_height * width / height
                    + up[2] * down * half_height,
            ];
            Ray {
                origin,
                direction: forward,
            }
        } else {
            let direction = [
                right[0] * across * tan * width / height + up[0] * down * tan + forward[0],
                right[1] * across * tan * width / height + up[1] * down * tan + forward[1],
                right[2] * across * tan * width / height + up[2] * down * tan + forward[2],
            ];
            Ray {
                origin: self.eye(),
                direction: normalize(direction, forward),
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{Camera, Mat4, look_at, perspective};

    /// A clip-space point as `[x, y]` in normalised device coordinates.
    fn ndc(matrix: &Mat4, point: [f32; 3]) -> [f32; 2] {
        let clip = matrix.project(point);
        [clip[0] / clip[3], clip[1] / clip[3]]
    }

    #[test]
    fn the_target_lands_in_the_middle_of_the_picture() {
        let camera = Camera::new([8.0, 8.0, 8.0], 32.0);
        let middle = ndc(&camera.view_projection(1.0, 16.0), camera.target);
        assert!(middle[0].abs() < 1e-4, "{middle:?}");
        assert!(middle[1].abs() < 1e-4, "{middle:?}");
    }

    #[test]
    fn a_point_above_the_target_is_higher_on_the_screen() {
        let camera = Camera::new([8.0, 8.0, 8.0], 32.0);
        let matrix = camera.view_projection(1.0, 16.0);
        let target = ndc(&matrix, camera.target);
        let above = ndc(&matrix, [8.0, 9.0, 8.0]);
        assert!(above[1] > target[1], "{above:?} is not above {target:?}");
    }

    #[test]
    fn orbiting_keeps_the_camera_above_the_horizon() {
        let mut camera = Camera::new([0.0; 3], 10.0);
        camera.orbit(20.0, 200.0);
        assert!(camera.pitch < 90.0);
        camera.orbit(0.0, -400.0);
        assert!(camera.pitch > -90.0);
    }

    #[test]
    fn zooming_moves_the_eye_and_keeps_the_direction() {
        let mut camera = Camera::new([0.0; 3], 10.0);
        let before = camera.eye();
        camera.zoom(0.5);
        let after = camera.eye();
        assert!((after[1] / before[1] - 0.5).abs() < 1e-5);
    }

    #[test]
    fn an_orthographic_view_has_no_foreshortening() {
        let mut camera = Camera::new([0.0; 3], 20.0);
        camera.yaw = 0.0;
        camera.pitch = 0.0;
        camera.orthographic = true;
        let flat = camera.view_projection(1.0, 8.0);
        let near = ndc(&flat, [1.0, 0.0, 0.0]);
        let far = ndc(&flat, [1.0, 0.0, -5.0]);
        assert!(
            (near[0] - far[0]).abs() < 1e-4,
            "the far point is the same size: {near:?} against {far:?}"
        );

        /* A perspective view does narrow with depth, which is what makes the
         * two projections different. */
        camera.orthographic = false;
        let deep = camera.view_projection(1.0, 8.0);
        assert!((ndc(&deep, [1.0, 0.0, 0.0])[0] - ndc(&deep, [1.0, 0.0, -5.0])[0]).abs() > 1e-3);
    }

    #[test]
    fn a_look_at_puts_the_eye_at_the_origin_of_view_space() {
        let view = look_at([0.0, 0.0, 10.0], [0.0; 3], [0.0, 1.0, 0.0]);
        let clip = view.project([0.0, 0.0, 10.0]);
        assert!(clip[0].abs() < 1e-5 && clip[1].abs() < 1e-5);
        /* In front of the camera is view-space -z. */
        let ahead = view.project([0.0, 0.0, 5.0]);
        assert!(ahead[2] < 0.0);
    }

    #[test]
    fn fitting_a_box_keeps_every_corner_on_the_picture() {
        let (min, max) = ([0.0, 0.0, 0.0], [16.0, 1.0, 1.0]);
        for aspect in [4.0, 1.0, 0.5] {
            let mut camera = Camera::new([0.0; 3], 1.0);
            camera.fit(min, max, aspect);
            assert_eq!(camera.target, [8.0, 0.5, 0.5]);
            let matrix = camera.view_projection(aspect, 0.5 * 16.0);
            for corner in 0..8 {
                let point = [
                    if corner & 1 == 0 { min[0] } else { max[0] },
                    if corner & 2 == 0 { min[1] } else { max[1] },
                    if corner & 4 == 0 { min[2] } else { max[2] },
                ];
                let at = ndc(&matrix, point);
                assert!(
                    at[0].abs() <= 1.0 && at[1].abs() <= 1.0,
                    "aspect {aspect}: {point:?} is at {at:?}"
                );
            }
        }
    }

    #[test]
    fn a_perspective_keeps_a_far_point_inside_the_clip() {
        let matrix = perspective(45.0, 1.0, 0.1, 100.0);
        let clip = matrix.project([0.0, 0.0, -50.0]);
        assert!(clip[3] > 0.0);
        assert!(clip[2] / clip[3] > -1.0 && clip[2] / clip[3] < 1.0);
    }
}
