/* eslint-disable */
var path = require('path');


module.exports = {
  entry: './javascript/peaklimiter.js',
  output: {
    path: path.resolve(__dirname, 'dist'),
    filename: 'peaklimiter.js',
  },
  module: {
    loaders: [
      {
        test: /\.js?$/,
        loader: 'babel',
      },
    ],
  },
  eslint: {
    formatter: require('eslint-friendly-formatter'),
    failOnError: false,
  }
};
