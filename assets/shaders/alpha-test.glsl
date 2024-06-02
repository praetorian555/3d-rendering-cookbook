void RunAlphaTest(float alpha, float alpha_threshold)
{
    if (alpha_threshold > 0.0)
    {
        // http://alex-charlton.com/posts/Dithering_on_the_GPU/
        // https://forums.khronos.org/showthread.php/5091-screen-door-transparency
        mat4 threshold_matrix = mat4(
        1.0  / 17.0,  9.0 / 17.0,  3.0 / 17.0, 11.0 / 17.0,
        13.0 / 17.0,  5.0 / 17.0, 15.0 / 17.0,  7.0 / 17.0,
        4.0  / 17.0, 12.0 / 17.0,  2.0 / 17.0, 10.0 / 17.0,
        16.0 / 17.0,  8.0 / 17.0, 14.0 / 17.0,  6.0 / 17.0
        );

        alpha = clamp(alpha - 0.5 * threshold_matrix[int(mod(gl_FragCoord.x, 4.0))][int(mod(gl_FragCoord.y, 4.0))], 0.0, 1.0);

        if (alpha < alpha_threshold)
        {
            discard;
        }
    }
}