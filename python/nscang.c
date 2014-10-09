/*
 * Copyright (c) 2014 Alexander Golovko <alexandro@onsec.ru>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Python.h>
#include "structmember.h"

#include <openssl/ssl.h>

#include "client.h"

typedef struct {
	PyObject_HEAD char *host;
	unsigned int port;
	char *identity;
	char *psk;
	char *ciphers;
	nscang_client_t *client;
	/* Type-specific fields go here. */
} NSCAngNotifyer;

static PyObject *
nscang_host_result(PyObject *self, PyObject *args, PyObject *kwds)
{
	char errstr[1024];
	static char *kwlist[] = { "host_name", "return_code", "plugin_output",
	    "timeout", NULL };
	char *host = NULL;
	char *output = "";
	int rc = 0;
	int timeout = 5;
	nscang_client_t *client = ((NSCAngNotifyer *)self)->client;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sI|sI:host_result",
	    kwlist, &host, &rc, &output, &timeout))
		return NULL;

	if (nscang_client_send_push(client, host, NULL, rc, output, timeout)) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	nscang_client_disconnect(client);

	if (nscang_client_send_push(client, host, NULL, rc, output, timeout)) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	PyErr_Format(PyExc_RuntimeError, "svc_result: %s",
	    nscang_client_errstr(client, errstr, sizeof(errstr)));
	return NULL;
}

static PyObject *
nscang_svc_result(PyObject *self, PyObject *args, PyObject *kwds)
{
	char errstr[1024];
	static char *kwlist[] = { "host_name", "svc_description", "return_code",
	    "plugin_output", "timeout", NULL };
	char *host = NULL;
	char *service = NULL;
	char *output = "";
	int rc = 0;
	int timeout = 5;
	nscang_client_t *client = ((NSCAngNotifyer *)self)->client;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "ssI|sI:svc_result",
	    kwlist, &host, &service, &rc, &output, &timeout))
		return NULL;

	if (nscang_client_send_push(client, host, service, rc, output, timeout)) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	nscang_client_disconnect(client);

	if (nscang_client_send_push(client, host, service, rc, output, timeout)) {
		Py_INCREF(Py_None);
		return Py_None;
	}

	PyErr_Format(PyExc_RuntimeError, "svc_result: %s",
	    nscang_client_errstr(client, errstr, sizeof(errstr)));
	return NULL;
}

static int
NSCAngNotifyer_init(NSCAngNotifyer *self, PyObject *args, PyObject *kwds)
{
	char errstr[1024];
	static char *kwlist[] = { "host", "port", "identity", "psk", "ciphers",
	    NULL };

	self->host = NULL;
	self->port = 5668;
	self->identity = NULL;
	self->psk = NULL;
	self->ciphers = NULL;

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "sIss|s:__init__", kwlist,
	    &self->host, &self->port, &self->identity,
	    &self->psk, &self->ciphers))
		return -1;

	self->client = malloc(sizeof(nscang_client_t));
	if (self->client == NULL) {
		PyErr_Format(PyExc_MemoryError,
		    "Can't allocate memory for NSCAng connection state");
		return -1;
	}

	if (!nscang_client_init(self->client, self->host, self->port,
		self->ciphers, self->identity, self->psk)) {
		PyErr_Format(PyExc_RuntimeError, "nscang_client_init: %s",
		    nscang_client_errstr(self->client, errstr, sizeof(errstr)));
		return -1;
	}

	return 0;
}

static void
NSCAngNotifyer_dealloc(NSCAngNotifyer *self)
{
	nscang_client_free(self->client);
}

static PyMemberDef NSCAngNotifyer_members[] = {
	{ "host", T_STRING, offsetof(NSCAngNotifyer, host), READONLY },
	{ "port", T_INT, offsetof(NSCAngNotifyer, port), READONLY },
	{ NULL }
};

static PyMethodDef NSCAngNotifyer_methods[] = {
	{ "host_result", (PyCFunction) nscang_host_result,
	    METH_VARARGS | METH_KEYWORDS,
	    "Send a passive host check to the configured monitoring host" },
	{ "svc_result", (PyCFunction) nscang_svc_result,
	    METH_VARARGS | METH_KEYWORDS,
	    "Send a service result to the configured monitoring host" },
	{ NULL }
};

static PyTypeObject NSCAngNotifyerType = {
	PyObject_HEAD_INIT(NULL) 0,             /* ob_size */
	"nscang.NSCAngNotifyer",                /* tp_name */
	sizeof(NSCAngNotifyer),                 /* tp_basicsize */
	0,                                      /* tp_itemsize */
	(destructor) NSCAngNotifyer_dealloc,    /* tp_dealloc */
	0,                                      /* tp_print */
	0,                                      /* tp_getattr */
	0,                                      /* tp_setattr */
	0,                                      /* tp_compare */
	0,                                      /* tp_repr */
	0,                                      /* tp_as_number */
	0,                                      /* tp_as_sequence */
	0,                                      /* tp_as_mapping */
	0,                                      /* tp_hash  */
	0,                                      /* tp_call */
	0,                                      /* tp_str */
	0,                                      /* tp_getattro */
	0,                                      /* tp_setattro */
	0,                                      /* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,                     /* tp_flags */
	"NSCAngNotifyer objects",               /* tp_doc */
	0,                                      /* tp_traverse */
	0,                                      /* tp_clear */
	0,                                      /* tp_richcompare */
	0,                                      /* tp_weaklistoffset */
	0,                                      /* tp_iter */
	0,                                      /* tp_iternext */
	NSCAngNotifyer_methods,                 /* tp_methods */
	NSCAngNotifyer_members,                 /* tp_members */
	0,                                      /* tp_getset */
	0,                                      /* tp_base */
	0,                                      /* tp_dict */
	0,                                      /* tp_descr_get */
	0,                                      /* tp_descr_set */
	0,                                      /* tp_dictoffset */
	(initproc) NSCAngNotifyer_init,         /* tp_init */
	PyType_GenericAlloc,                    /* tp_alloc */
	PyType_GenericNew,                      /* tp_new */
	_PyObject_Del                           /* tp_free */
};

static PyMethodDef module_methods[] = {
	{ NULL }
};

DL_EXPORT(void)
initnscang(void)
{
	PyObject *m;

	SSL_library_init();
	SSL_load_error_strings();

	if (PyType_Ready(&NSCAngNotifyerType) < 0)
		return;

	m = Py_InitModule("nscang", module_methods);
	if (m == NULL)
		return;

	Py_INCREF(&NSCAngNotifyerType);
	PyModule_AddObject(m, "NSCAngNotifyer",
	    (PyObject *)&NSCAngNotifyerType);
}
