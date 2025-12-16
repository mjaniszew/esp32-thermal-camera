const path = require('path');
const HtmlWebpackPlugin = require('html-webpack-plugin');
const TerserPlugin = require('terser-webpack-plugin');

module.exports = {
  mode: "production",
  entry: "./dummy.js",
  output: {
    path: path.resolve(__dirname, 'dist'),
    clean: true,
  },
  plugins: [
    new HtmlWebpackPlugin({
      template: path.resolve(__dirname, 'src', 'index.html'),
      filename: 'index.html', 
      minify: {
        collapseWhitespace: true, 
        removeComments: true,
        removeRedundantAttributes: true,
        useShortDoctype: true,
        minifyJS: true, 
      },
    }),
  ],
  optimization: {
    minimize: true,
    minimizer: [
      new TerserPlugin({}),
    ],
  },
  resolve: {
    extensions: ['.js'],
  },
};