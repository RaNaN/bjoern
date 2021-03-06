#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"
#include "filewrapper.h"

// indicates tls is activated
bool tls = false;

PyDoc_STRVAR(listen_doc,
"listen(application, host, port) -> None\n\n \
\
Makes bjoern listen to host:port and use application as WSGI callback. \
(This does not run the server mainloop.)");
static PyObject*
listen(PyObject* self, PyObject* args)
{
  const char* host;
  int port;

  if(wsgi_app) {
    PyErr_SetString(
      PyExc_RuntimeError,
      "Only one bjoern server per Python interpreter is allowed"
    );
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "Osi:run/listen", &wsgi_app, &host, &port))
    return NULL;
  
  PyObject* key_obj = PyObject_GetAttrString(bjoern_module, "key_path");
  PyObject* cert_obj = PyObject_GetAttrString(bjoern_module, "cert_path");
  
#ifdef TLS_SUPPORT
  char *key, *cert;
  if(PyString_Check(key_obj) && PyString_Check(cert_obj)) {
    tls = true;
    key = PyString_AsString(key_obj);
    cert = PyString_AsString(cert_obj);
  }
#endif
  
   _initialize_request_module(host, port, tls);
  
  if(tls){
#if TLS_SUPPORT

   char* ciphers = NULL;
   PyObject* ciphers_obj = PyObject_GetAttrString(bjoern_module, "ciphers");
   
   if(PyString_Check(ciphers_obj)) {
     ciphers =  PyString_AsString(ciphers_obj);
  }

    if(!create_tls_context(key, cert, ciphers)) {
    PyErr_Format(
      PyExc_RuntimeError,
      "Could not create tls context"
    );
    return NULL; 
    }
#endif
  } 
  if(!server_init(host, port)) {
    PyErr_Format(
      PyExc_RuntimeError,
      "Could not start server on %s:%d", host, port
    );
    return NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(run_doc,
"run(application, host, port) -> None\n \
Calls listen(application, host, port) and starts the server mainloop.\n \
\n\
run() -> None\n \
Starts the server mainloop. listen(...) has to be called before calling \
run() without arguments.");
static PyObject*
run(PyObject* self, PyObject* args)
{
  if(PyTuple_GET_SIZE(args) == 0) {
    /* bjoern.run() */
    if(!wsgi_app) {
      PyErr_SetString(
        PyExc_RuntimeError,
        "Must call bjoern.listen(app, host, port) before "
        "calling bjoern.run() without arguments."
      );
      return NULL;
    }
  } else {
    /* bjoern.run(app, host, port) */
    if(!listen(self, args))
      return NULL;
  }

  server_run();
  wsgi_app = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
  {"run", run, METH_VARARGS, run_doc},
  {"listen", listen, METH_VARARGS, listen_doc},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initbjoern()
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  bjoern_module = Py_InitModule("bjoern", Bjoern_FunctionTable);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(ii)", 1, 3));
  PyModule_AddObject(bjoern_module, "key_path", Py_BuildValue("s", NULL));
  PyModule_AddObject(bjoern_module, "cert_path", Py_BuildValue("s", NULL));
  PyModule_AddObject(bjoern_module, "ciphers", Py_BuildValue("s", NULL));
#if TLS_SUPPORT
  PyModule_AddObject(bjoern_module, "tls", Py_True);
#else
  PyModule_AddObject(bjoern_module, "tls", Py_False);
#endif
  
}
